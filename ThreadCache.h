#pragma once
#include"Common.h"
#include"CentralCache.h"

class ThreadCache
{
private:
    FreeList _freeList[NFREELIST];

public:
    void* Allocate(size_t size);

    void Deallocate(void* ptr, size_t size);

    void* FetchFromCentralCache(size_t index, size_t size);

    void ListTooLong(FreeList& list, size_t size);

};
static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;