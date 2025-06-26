#pragma once
#include"ThreadCache.h"
#include"ObjectPool.h"
static void* ConcurrentAlloc(size_t size)
{
    if (size > MAX_BYTES)
    {
        size_t alignNum = SizeClass::RoundUp(size);
        size_t kpage = alignNum >> PAGE_SHIFT;//获得k页的span
        PageCache::GetInstance()->GetPageMutex().lock();
        Span* span=PageCache::GetInstance()->NewSpan(kpage);
        span->_objSize = size;
        PageCache::GetInstance()->GetPageMutex().unlock();
        void* ptr = (void*)(span->_pageid << PAGE_SHIFT);
        return ptr;
    }
    else
    {
        // 获取每个线程自己的ThreadCache对象
        if (pTLSThreadCache == nullptr) {
            ObjectPool<ThreadCache> tcPool;
            pTLSThreadCache = tcPool.New();
        }
        return pTLSThreadCache->Allocate(size);
    }
    
    
}
static void ConcurrentFree(void* ptr)
{
    Span* span=PageCache::GetInstance()->MapObjectToSpan(ptr);
    size_t size = span->_objSize;
    if (size > MAX_BYTES)
    {
        Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);

        PageCache::GetInstance()->GetPageMutex().lock();
        PageCache::GetInstance()->ReleaseSpanToPageCache(span);
        PageCache::GetInstance()->GetPageMutex().unlock();
    }
    else
    {
        assert(pTLSThreadCache);
        pTLSThreadCache->Deallocate(ptr, size);
    }
    
}