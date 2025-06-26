#include "CentralCache.h"
CentralCache CentralCache::_sInst;
Span* CentralCache::GetOneSpan(SpanList& list, size_t byte_size)
{
    //��ȡһ���ǿ�span
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
    //�Ƚ�centralcache��Ͱ���ͷŵ������������߳̽��ڴ��ͷŻ�����ʱ�򲻻�����
    list.GetMutex().unlock();
    //û�ҵ�һ���ǿ�Span����������pagecache����
    PageCache::GetInstance()->GetPageMutex().lock();//��Ϊ�ǻ�ȡpagecache��span�����Ҫ��pagecache����
    Span* span=PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(byte_size));
    PageCache::GetInstance()->GetPageMutex().unlock();

    //�������ﲻ��Ҫ��������Ϊspan��ʱֻ�е�ǰ�߳��ܹ����ʵ�����û�з��뵽�����У���������̷߳��ʲ���
    char* page_start = (char*)(span->_pageid << PAGE_SHIFT);//����ҳ�Ż�ȡҳ����ʼ��ַ

    size_t bytes = span->_n << PAGE_SHIFT;//����span���ڴ��С��ռ���ֽ������൱����span��ҳ������*ҳ�Ĵ�С
    char* end = page_start + bytes;

    assert(byte_size >= sizeof(void*));

    //������ڴ��г��������������
    span->_freeList = page_start;//��������һ��ҵ�������
    page_start += byte_size;
    void* tail = span->_freeList;//β���
    while (page_start + byte_size <= end)
    {
        NodeNext(tail) = page_start;
        tail = page_start; // Ҳ����д��tail = page_start
        page_start += byte_size;
    }
    NodeNext(tail) = nullptr;
    //�����spanͷ����list��
    //��span���뵽list�к������߳̾��ܷ��ʵ��ˣ������Ҫ�������¼���
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

    //��span�л�ȡn�������ж����ö���
    start = span->_freeList;
    end = start;
    size_t i = 0;
    size_t retNum = 1;//�ʼ��start�൱���Ѿ�����һ����
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
    //�����ڴ���ȡҪ�ŵ��ĸ�span��
    while (start)//��startΪ��ʱ�ͽ��������ȫ��������centralcache
    {
        void* next = NodeNext(start);
        Span* span=PageCache::GetInstance()->MapObjectToSpan(start);//����ҳ�Ż�õ��������ĸ�span����������ڴ�
        NodeNext(start) = span->_freeList;//���ڴ����ӵ�span��freelist��
        span->_freeList = start;
        span->_useCount--;
        if (span->_useCount == 0)
        {
            //��ʱ˵�����Span�����ȥ���ڴ��Ѿ�ȫ���������ˣ����Ի��ظ�PageCache��
            _spanlist[index].Erase(span);//��CentralCache���ٹ������span
            span->_freeList = nullptr;
            span->_next = nullptr;
            span->_prev = nullptr;
            //�Ƚ�CentralCache��Ͱ���⿪����Ϊ��ʱ��Ҫ������CentralCache�����Span����PageCache������Ҫ����CentralCache������
            //ͬʱ����Ҳ����������Ҫ��Ͱ�������߳��ܹ��������������⵱ǰ�̲߳���PageCacheʱ�����̵߳ȴ�
            _spanlist[index].GetMutex().unlock();
            //pagecache��һ�Ѵ��������ֱ�ӽ�������ס����
            PageCache::GetInstance()->GetPageMutex().lock();
            PageCache::GetInstance()->ReleaseSpanToPageCache(span);//��span���ظ�pagecache
            PageCache::GetInstance()->GetPageMutex().unlock();
            _spanlist[index].GetMutex().lock();
        }

        start = next;
    }

    _spanlist[index].GetMutex().unlock();
}