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
        it = it->_next;
    }
    //û�ҵ�һ���ǿ�Span����������pagecache����
    Span* span=PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(byte_size));
    char* page_start = (char*)(span->_pageid << PAGE_SHIFT);//����ҳ�Ż�ȡҳ����ʼ��ַ
    size_t bytes = span->_n << PAGE_SHIFT;//����span���ڴ��С��ռ���ֽ����������൱�ڳ�2^13
    char* end = page_start + bytes;
    //������ڴ��г��������������
    span->_freeList = page_start;//��������һ��ҵ�������
    page_start += byte_size;
    void* tail = span->_freeList;//β���
    while (page_start < end)
    {
        NodeNext(tail) = page_start;
        tail = page_start;//Ҳ����д��tail=NodeNext(tail)
        page_start += byte_size;
    }
    //�����spanͷ����list��
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

    _spanlist[index].GetMutex().unlock();
    return retNum;
}