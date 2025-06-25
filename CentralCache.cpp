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
        it = it->_next;
    }
    //没找到一个非空Span，接下来向pagecache申请
    Span* span=PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(byte_size));
    char* page_start = (char*)(span->_pageid << PAGE_SHIFT);//根据页号获取页的起始地址
    size_t bytes = span->_n << PAGE_SHIFT;//计算span的内存大小所占的字节数，左移相当于乘2^13
    char* end = page_start + bytes;
    //将大块内存切成自由链表挂起来
    span->_freeList = page_start;//先切下来一块挂到链表上
    page_start += byte_size;
    void* tail = span->_freeList;//尾结点
    while (page_start < end)
    {
        NodeNext(tail) = page_start;
        tail = page_start;//也可以写成tail=NodeNext(tail)
        page_start += byte_size;
    }
    //将这个span头插入list中
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

    _spanlist[index].GetMutex().unlock();
    return retNum;
}