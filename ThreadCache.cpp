#include"ThreadCache.h"

void* ThreadCache::Allocate(size_t size)
{
    assert(size <= MAX_BYTES);
    size_t alignSize = SizeClass::RoundUp(size);
    size_t index = SizeClass::Index(alignSize);
    if (!_freeList[index].Empty())
    {
        return _freeList[index].Pop();
    }
    else
    {
        return FetchFromCentralCache(index,alignSize);
    }
}
void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{

}
void ThreadCache::Deallocate(void* ptr, size_t size)
{
    
}



