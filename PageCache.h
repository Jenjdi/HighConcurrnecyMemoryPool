#pragma once
#include"Common.h"
class PageCache
{
private:
    static PageCache _sInst;
    std::mutex _mutex;
    SpanList _slist[NPAGES];
    PageCache() { }
    PageCache(const PageCache&) = delete;
    

public:
    static PageCache* GetInstance()
    {
        return &_sInst;
    }
    Span* NewSpan(size_t size);
};