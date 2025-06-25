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
    //��������������
    //sizeԽ��һ�����������Խ��
    //sizeԽС��һ�����������Խ��
    size_t batchNum = min(_freeList[index].MaxSize(), SizeClass::NumMoveSize(size));
    if (batchNum == _freeList[index].MaxSize()) {
        _freeList[index].MaxSize()++;
    }
    void* start = nullptr;
    void* end = nullptr;
    size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, size);
    //��������ͽ����е���������
    assert(actualNum > 1);
    //���쳣�����ϲ㴦��
    if (actualNum == 1)
    {
        assert(start == end);
        return start;
    }
    _freeList[index].PushRange(NodeNext(start), end); // start��Ҫ���أ����Խ�start����Ķ���push��������
    return start;

    
}
void ThreadCache::Deallocate(void* ptr, size_t size)
{
    assert(ptr);
    assert(size <= MAX_BYTES);
    //�ҵ�ӳ���Ͱ��λ�ã�Ȼ���ڴ����
    size_t index = SizeClass::Index(size);
    _freeList[index].Push(ptr);
}



