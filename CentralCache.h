#pragma once
#include"Common.h"
#include"PageCache.h"
class CentralCache
{
private:
    SpanList _spanlist[NFREELIST];//���ƹ�ϣͰ���������Ϸ�����
    static CentralCache _sInst;
    CentralCache() { }
    CentralCache(const CentralCache&) = delete;

public:
    static CentralCache* GetInstance()
    {
        return &_sInst;
    }
    //��ȡһ�������Ķ����ThreadCache
    size_t FetchRangeObj(void*& start, void*& end, size_t n, size_t byte_size);
    Span* GetOneSpan(SpanList& list,size_t byte_size);
    void ReleaseListToSpan(void* start, size_t size);
};