#pragma once
#include<iostream>
#include<vector>
#include<mutex>
#include<thread>
#include<algorithm>
#include<ctime>
#include<cassert>

static const size_t MAX_BYTES = 256 * 1024; // thread_cache最大256KB
static const size_t NFREELIST = 208;
static const size_t NPAGES = 129;//实际上使用到的就是128个，为了让下标和页数一一对应，将0下标空出来
static const size_t PAGE_SHIFT = 13;//页大小为2^13
#ifdef _WIN64
typedef unsigned long long PAGE_ID;
#elif _WIN32
typedef size_t PAGE_ID;
#else
//Linux

#endif

#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/mman.h>
#endif

inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32

    void* ptr = VirtualAlloc(0, kpage * (1 << 12), MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE);
#else
    // linux下brk mmap等
    void* ptr = mmap(nullptr, kpage, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

#endif
    if (ptr == nullptr)
        throw std::bad_alloc();
    return ptr;
}

inline void*& NodeNext(void* obj)
{
    return *(void**)obj;
}
class FreeList
{
private:
    void* _freeList = nullptr;
    size_t _MaxSize = 1;

public:
    void Push(void* obj)
    {
        NodeNext(obj) = _freeList;
        _freeList = obj;
    }
    void PushRange(void* begin, void* end)
    {
        NodeNext(end) = _freeList;
        _freeList = begin;
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
    size_t& MaxSize()
    {
        return _MaxSize;
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
    //ThreadCache一次从CentralCache中取出多少个
    static size_t NumMoveSize(size_t size)
    {
        // [2, 512]，一次批量移动多少个对象的(慢启动)上限值
        // 小对象一次批量上限高
        // 小对象一次批量上限低
        assert(size > 0);
        size_t Num = MAX_BYTES / size;//一次取出对象的数量
        //不能太大也不能太小
        if (Num < 2)
        {
            Num = 2;
        }
        if (Num > 512)
        {
            Num = 512;
        }
        return Num;
    }

    static size_t NumMovePage(size_t size)
    {
        // 计算一次向系统获取几个页
        // 单个对象 8byte
        // ...
        // 单个对象 256KB
        size_t num = NumMoveSize(size);
        size_t npage = num * size;//num个对象，一个对象size个字节，算出总的字节数
        npage >>= PAGE_SHIFT;//一个页大小为2^PAGE_SHIFT字节，通过右移能更高效的算出具体所需要的页数
        if (npage == 0)
            npage = 1;
        return npage;
    }
};

    //管理多个连续页大块内存的跨度
struct Span
{
    //内存大小 = _n * 页大小
    PAGE_ID _pageid=0;//大块内存的起始页号
    size_t _n=0;//页的数量
    Span* _prev=nullptr;
    Span* _next=nullptr;
    
    size_t _useCount=0;//切好的小块内存，被分配给thread cache的计数，当全部收回时就为0，同时默认也是0
    void* _freeList=nullptr;//切好的小块内存的自由链表

    size_t _maxSize = 1;
};
class SpanList//带头双向链表
{
private:
    Span* _head = nullptr;
    std::mutex _mutex;//桶锁

public:
    SpanList()
    {
        _head = new Span;
        _head->_next = _head;
        _head->_prev = _head;
    }
    std::mutex& GetMutex()
    {
        return _mutex;
    }
    Span* Begin()
    {
        return _head->_next;
    }
    Span* End()
    {
        return _head;
    }
    void PushFront(Span* span)
    {
        Insert(Begin(), span);
    }
    void Insert(Span* pos, Span* newspan)
    {
        assert(pos);
        assert(newspan);
        Span* prev = pos->_prev;
        prev->_next = newspan;
        newspan->_prev = prev;
        pos->_prev=newspan;
        newspan->_next = pos;
    }
    Span* PopFront()
    {
        Span* front = _head->_next;
        Erase(front);//只是将front取出来了，并没有将内存释放
        return front;
    }
    bool Empty()
    {
        return _head == _head->_next;
    }
    void Erase(Span* pos)
    {
        assert(pos);
        assert(pos != _head);
        Span* prev = pos->_prev;
        Span* next = pos->_next;
        prev->_next = next;
        next->_prev = prev;
    }
};