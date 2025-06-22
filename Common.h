#pragma once
#include<iostream>
#include<vector>
#include<ctime>
#include<cassert>
const static int MAX_BYTES = 256 * 1024;//thread_cache最大256KB
const static int NFREELIST = 208;
void*& NodeNext(void* obj)
{
    return *(void**)obj;
}
class FreeList
{
private:
    void* _freeList;

public:
    void Push(void* obj)
    {
        NodeNext(obj) = _freeList;
        _freeList = obj;
    }
    void* Pop()
    {
        void* obj = _freeList;
        _freeList = NodeNext(obj);
        return obj;
    }
    bool Empty()
    {
        return _freeList == nullptr;
    }
};
//管理内存对其和映射

class SizeClass {
private:
    static inline size_t _RoundUP(size_t size, size_t alignNum)
    {
        // 返回为了满足对齐规则而实际分配的内存值
        // size_t alignSize;
        // if (size % 8 != 0)
        //{
        //     alignSize = (size / alignNum + 1) * alignNum;
        //     //向上对齐，如10：10比8大，比下一个对齐数16小，因此直接对齐变成16
        // }
        // else
        //{
        //     alignSize = size;
        // }
        // return alignSize;
        return (((size) + alignNum - 1) & ~(alignNum - 1));
        // 将所以不足对齐数的值都补足到对齐数
    }
    static inline size_t _Index(size_t bytes, size_t align_shift)
    {
        return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
    }

public:
    // 整体控制在最多10%左右的内碎片浪费
    // [1,128]              8byte对齐 freelist[0,16)
    // [128+1,1024]         16byte对齐 freelist[16,72)
    // [1024+1,8*1024]      128byte对齐 freelist[72,128)
    // [8*1024+1,64*1024]   1024byte对齐 freelist[128,184)
    // [64*1024+1,256*1024] 8*1024byte对齐 freelist[184,208)
    //不足对齐数量倍数的补足到对应对齐数量的整数倍
    //如：想要分配129字节的内存，由于不是16字节的整数倍，因此加上15字节补足到144，这里的15字节就是浪费的
    
    static inline size_t RoundUp(size_t size)
    {
        if (size <= 128)
        {
            return _RoundUP(size, 8); // 按照8字节对齐
        }
        else if (size <= 1024)
        {
            return _RoundUP(size, 16);
        }
        else if (size<=8*1024)
        {
            return _RoundUP(size, 128);
        }
        else if (size <= 64 * 1024)
        {
            return _RoundUP(size, 1024);
        }
        else if (size <= 256 * 1024)
        {
            return _RoundUP(size, 8 * 1024);
        }
        else
        {
            //错误处理，一旦走到这里说明出错了
            assert(false);
            return -1;
        }
    }

    //查找当前所在是哪个的桶
    static inline size_t Index(size_t bytes)
    {
        assert(bytes <= MAX_BYTES);
        // 每个区间有多少个链
        static int group_array[4] = { 16, 56, 56, 56 };
        if (bytes <= 128)
        {
            return _Index(bytes, 3);
        }
        else if (bytes <= 1024)
        {
            return _Index(bytes - 128, 4) + group_array[0];
        }
        else if (bytes <= 8 * 1024)
        {
            return _Index(bytes - 1024, 7) + group_array[1] + group_array[0];
        }
        else if (bytes <= 64 * 1024)
        {
            return _Index(bytes - 8 * 1024, 10) + group_array[2] + group_array[1] + group_array[0];
        }
        else if (bytes <= 256 * 1024)
        {
            return _Index(bytes - 64 * 1024, 13) + group_array[3] + group_array[2] + group_array[1] + group_array[0];
        }
        else
        {
            assert(false);
        }
        return -1;
    }
};