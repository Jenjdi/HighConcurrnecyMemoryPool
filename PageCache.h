#pragma once
#include"Common.h"
#include"ObjectPool.h"
class PageCache
{
private:
    static PageCache _sInst;
    std::mutex _mutex;
    SpanList _slist[NPAGES];
    std::unordered_map<PAGE_ID, Span*> _idSpanMap;
    ObjectPool<Span> _spanPool;
    PageCache() { }
    PageCache(const PageCache&) = delete;
    

public:
    static PageCache* GetInstance()
    {
        return &_sInst;
    }
    std::mutex& GetPageMutex()
    {
        return _mutex;
    }
    Span* NewSpan(size_t size);
    Span* MapObjectToSpan(void* obj);
    void ReleaseSpanToPageCache(Span* span);
};