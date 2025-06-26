#include "CentralCache.h"
CentralCache CentralCache::_sInst;
Span* CentralCache::GetOneSpan(SpanList& list, size_t byte_size)
{
    //获取一个非空span
    Span* it = list.Begin();
    while (it != list.End())
    {
        if (it->_freeList)
        {
            return it;
        }
        else
        {
            it = it->_next;
        }
        
    }
    //先将centralcache的桶锁释放掉，这样其他线程将内存释放回来的时候不会阻塞
    list.GetMutex().unlock();
    //没找到一个非空Span，接下来向pagecache申请
    PageCache::GetInstance()->GetPageMutex().lock();//因为是获取pagecache的span，因此要对pagecache加锁
    Span* span=PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(byte_size));
    PageCache::GetInstance()->GetPageMutex().unlock();

    //下面这里不需要加锁，因为span暂时只有当前线程能够访问到，并没有放入到链表中，因此其他线程访问不到
    char* page_start = (char*)(span->_pageid << PAGE_SHIFT);//根据页号获取页的起始地址

    size_t bytes = span->_n << PAGE_SHIFT;//计算span的内存大小所占的字节数，相当于用span中页的数量*页的大小
    char* end = page_start + bytes;

    assert(byte_size >= sizeof(void*));

    //将大块内存切成自由链表挂起来
    span->_freeList = page_start;//先切下来一块挂到链表上
    page_start += byte_size;
    void* tail = span->_freeList;//尾结点
    while (page_start + byte_size <= end)
    {
        NodeNext(tail) = page_start;
        tail = page_start; // 也可以写成tail = page_start
        page_start += byte_size;
    }
    NodeNext(tail) = nullptr;
    //将这个span头插入list中
    //将span插入到list中后其他线程就能访问到了，因此需要将锁重新加上
    list.GetMutex().lock();
    list.PushFront(span);

    return span;
}
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t n, size_t byte_size)
{
    size_t index = SizeClass::Index(byte_size);
    _spanlist[index].GetMutex().lock();

    Span* span = GetOneSpan(_spanlist[index], byte_size);
    assert(span);
    assert(span->_freeList);

    //从span中获取n个对象，有多少拿多少
    start = span->_freeList;
    end = start;
    size_t i = 0;
    size_t retNum = 1;//最开始的start相当于已经拿了一个了
    while (i < n - 1 && NodeNext(end) != nullptr)
    {
        end = NodeNext(end);
        i++;
        retNum++;
    }
    span->_freeList = NodeNext(end);
    NodeNext(end) = nullptr;
    span->_useCount += retNum;

    _spanlist[index].GetMutex().unlock();
    return retNum;
}
void CentralCache::ReleaseListToSpan(void* start, size_t size)
{
    size_t index = SizeClass::Index(size);
    _spanlist[index].GetMutex().lock();
    //根据内存块获取要放到哪个span中
    while (start)//当start为空时就将这个链表全部还给了centralcache
    {
        void* next = NodeNext(start);
        Span* span=PageCache::GetInstance()->MapObjectToSpan(start);//根据页号获得到具体是哪个span管理着这块内存
        NodeNext(start) = span->_freeList;//将内存块添加到span的freelist中
        span->_freeList = start;
        span->_useCount--;
        if (span->_useCount == 0)
        {
            //此时说明这个Span分配出去的内存已经全部还回来了，可以还回给PageCache了
            _spanlist[index].Erase(span);//让CentralCache不再管理这个span
            span->_freeList = nullptr;
            span->_next = nullptr;
            span->_prev = nullptr;
            //先将CentralCache的桶锁解开，因为此时需要将不被CentralCache管理的Span还给PageCache，不需要动到CentralCache的内容
            //同时解锁也会让其他想要对桶操作的线程能够正常操作，避免当前线程操作PageCache时其他线程等待
            _spanlist[index].GetMutex().unlock();
            //pagecache是一把大锁，因此直接将这里锁住即可
            PageCache::GetInstance()->GetPageMutex().lock();
            PageCache::GetInstance()->ReleaseSpanToPageCache(span);//将span还回给pagecache
            PageCache::GetInstance()->GetPageMutex().unlock();
            _spanlist[index].GetMutex().lock();
        }

        start = next;
    }

    _spanlist[index].GetMutex().unlock();
}