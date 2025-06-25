#include "CentralCache.h"
CentralCache CentralCache::_sInst;
Span* CentralCache::GetOneSpan(SpanList& list, size_t byte_size)
{
    //��ȡһ���ǿ�span
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