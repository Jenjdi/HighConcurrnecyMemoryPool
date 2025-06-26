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
    //慢启动反馈调节
    //size越大，一次申请的数量越少
    //size越小，一次申请的数量越大
    size_t batchNum = min(_freeList[index].MaxSize(), SizeClass::NumMoveSize(size));
    if (batchNum == _freeList[index].MaxSize()) {
        _freeList[index].MaxSize()++;
    }
    void* start = nullptr;
    void* end = nullptr;
    size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, size);
    //如果不够就将现有的数量返回
    assert(actualNum > 0);
    //抛异常交给上层处理
    if (actualNum == 1)
    {
        assert(start == end);
        return start;
    }
    else
    {
        _freeList[index].PushRange(NodeNext(start), end,actualNum-1); // start需要返回，所以将start后面的对象push进链表中
        return start;
    }
}
void ThreadCache::Deallocate(void* ptr, size_t size)
{
    assert(ptr);
    assert(size <= MAX_BYTES);
    //找到映射的桶的位置，然后将内存插入
    size_t index = SizeClass::Index(size);
    _freeList[index].Push(ptr);
    //当链表长度大于一次申请的内存时就还一段list给centralcache
    if (_freeList[index].Size() > _freeList[index].MaxSize())
    {
        ListTooLong(_freeList[index], size);
    }
}
void ThreadCache::ListTooLong(FreeList& list, size_t size)
{
    void* start = nullptr;
    void* end = nullptr;
    list.PopRange(start, end, list.MaxSize());
    CentralCache::GetInstance()->ReleaseListToSpan(start, size);
}



