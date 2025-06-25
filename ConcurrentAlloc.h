#pragma once
#include"ThreadCache.h"
static void* ConcurrentAlloc(size_t size)
{
    //获取每个线程自己的ThreadCache对象
    if (pTLSThreadCache == nullptr)
    {
        pTLSThreadCache = new ThreadCache;
    }
    return pTLSThreadCache->Allocate(size);
}
static void ConcurrentFree(void* ptr, size_t size)
{
    assert(pTLSThreadCache);

}