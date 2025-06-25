#pragma once
#include"ThreadCache.h"
static void* ConcurrentAlloc(size_t size)
{
    //��ȡÿ���߳��Լ���ThreadCache����
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