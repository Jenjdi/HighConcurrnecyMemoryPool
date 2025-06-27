#pragma once
#include <algorithm>
#include <cassert>
#include <ctime>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>


static const size_t MAX_BYTES = 256 * 1024; // thread_cache���256KB
static const size_t NFREELIST = 208;
static const size_t NPAGES = 129; // ʵ����ʹ�õ��ľ���128����Ϊ�����±��ҳ��һһ��Ӧ����0�±�ճ���
static const size_t PAGE_SHIFT = 13; // ҳ��СΪ2^13=8KB
#ifdef _WIN64
typedef unsigned long long PAGE_ID;
static const size_t SystemBits = 64;
#elif _WIN32
typedef size_t PAGE_ID;
static const size_t SystemBits = 32;
#else
// Linux
#include <bits/wordsize.h>
#if __WORDSIZE == 64
typedef unsigned long long PAGE_ID;
#elif __WORDSIZE == 32
typedef size_t PAGE_ID;
#endif
#endif

#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/mman.h>
#endif

inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32

    void* ptr = VirtualAlloc(0, kpage << PAGE_SHIFT, MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE);
#else
    // linux��brk mmap��
    void* ptr = mmap(nullptr, kpage << PAGE_SHIFT, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

#endif
    if (ptr == nullptr)
        throw std::bad_alloc();
    return ptr;
}
inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
// sbrk unmmap��
#endif
}

inline void*& NodeNext(void* obj)
{
    return *(void**)obj;
}
class FreeList {
private:
    void* _freeList = nullptr;
    size_t _MaxSize = 1;
    size_t _size = 0;

public:
    size_t Size()
    {
        return _size;
    }
    void Push(void* obj)
    {
        NodeNext(obj) = _freeList;
        _freeList = obj;
        ++_size;
    }
    void PushRange(void* begin, void* end, size_t n)
    {
        NodeNext(end) = _freeList;
        _freeList = begin;
        _size += n;
    }
    void PopRange(void*& begin, void*& end, size_t n)
    {
        assert(n < _size);
        begin = _freeList;
        end = begin;
        for (int i = 0; i < n - 1; i++) // endһ��ʼ���ڵ�һ�����棬���ʵ������n-1��
        {
            end = NodeNext(end);
        }
        _freeList = NodeNext(end);
        NodeNext(end) = nullptr;
        _size -= n;
    }
    void* Pop()
    {
        void* obj = _freeList;
        _freeList = NodeNext(obj);
        --_size;
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
// �����ڴ�����ӳ��

class SizeClass {
public:
    static inline size_t _RoundUP(size_t size, size_t alignNum)
    {
        // ����Ϊ�������������ʵ�ʷ�����ڴ�ֵ
        // size_t alignSize;
        // if (size % 8 != 0)
        //{
        //     alignSize = (size / alignNum + 1) * alignNum;
        //     //���϶��룬��10��10��8�󣬱���һ��������16С�����ֱ�Ӷ�����16
        // }
        // else
        //{
        //     alignSize = size;
        // }
        // return alignSize;
        return (((size) + alignNum - 1) & ~(alignNum - 1));
        // �����Բ����������ֵ�����㵽������
    }
    static inline size_t _Index(size_t bytes, size_t align_shift) // align_shift����С��64
    {
        return ((bytes + (1ULL << align_shift) - 1) >> align_shift) - 1;
    }

public:
    // ������������10%���ҵ�����Ƭ�˷�
    // [1,128]              8byte���� freelist[0,16)
    // [128+1,1024]         16byte���� freelist[16,72)
    // [1024+1,8*1024]      128byte���� freelist[72,128)
    // [8*1024+1,64*1024]   1024byte���� freelist[128,184)
    // [64*1024+1,256*1024] 8*1024byte���� freelist[184,208)
    // ����������������Ĳ��㵽��Ӧ����������������
    // �磺��Ҫ����129�ֽڵ��ڴ棬���ڲ���16�ֽڵ�����������˼���15�ֽڲ��㵽144�������15�ֽھ����˷ѵ�

    static inline size_t RoundUp(size_t size)
    {
        if (size <= 128) {
            return _RoundUP(size, 8); // ����8�ֽڶ���
        } else if (size <= 1024) {
            return _RoundUP(size, 16);
        } else if (size <= 8 * 1024) {
            return _RoundUP(size, 128);
        } else if (size <= 64 * 1024) {
            return _RoundUP(size, 1024);
        } else if (size <= 256 * 1024) {
            return _RoundUP(size, 8 * 1024);
        } else {
            // ����256k�����
            return _RoundUP(size, 1 << PAGE_SHIFT);
        }
    }

    // ���ҵ�ǰ�������ĸ���Ͱ
    static inline size_t Index(size_t bytes)
    {
        assert(bytes <= MAX_BYTES);
        // ÿ�������ж��ٸ���
        static int group_array[4] = { 16, 56, 56, 56 };
        if (bytes <= 128) {
            return _Index(bytes, 3);
        } else if (bytes <= 1024) {
            return _Index(bytes - 128, 4) + group_array[0];
        } else if (bytes <= 8 * 1024) {
            return _Index(bytes - 1024, 7) + group_array[1] + group_array[0];
        } else if (bytes <= 64 * 1024) {
            return _Index(bytes - 8 * 1024, 10) + group_array[2] + group_array[1] + group_array[0];
        } else if (bytes <= 256 * 1024) {
            return _Index(bytes - 64 * 1024, 13) + group_array[3] + group_array[2] + group_array[1] + group_array[0];
        } else {
            assert(false);
        }
        return -1;
    }
    // ThreadCacheһ�δ�CentralCache��ȡ�����ٸ�
    static size_t NumMoveSize(size_t size)
    {
        // [2, 512]��һ�������ƶ����ٸ������(������)����ֵ
        // С����һ���������޸�
        // С����һ���������޵�
        assert(size > 0);
        size_t Num = MAX_BYTES / size; // һ��ȡ�����������
        // ����̫��Ҳ����̫С
        if (Num < 2) {
            Num = 2;
        }
        if (Num > 512) {
            Num = 512;
        }
        return Num;
    }

    static size_t NumMovePage(size_t size)
    {
        // ����һ����ϵͳ��ȡ����ҳ
        // �������� 8byte
        // ...
        // �������� 256KB
        size_t num = NumMoveSize(size);
        size_t npage = num * size; // num������һ������size���ֽڣ�����ܵ��ֽ���
        npage >>= PAGE_SHIFT; // һ��ҳ��СΪ2^PAGE_SHIFT�ֽڣ�ͨ�������ܸ���Ч�������������Ҫ��ҳ��
        if (npage == 0)
            npage = 1; // ���ٷ���һ��ҳ
        return npage;
    }
};

// ����������ҳ����ڴ�Ŀ��
struct Span {
    // ҳ��������ķ��䵥λ��һ��ҳ8KB
    // �ڴ��С = _n * ҳ��С
    PAGE_ID _pageid = 0; // ����ڴ����ʼҳ��
    size_t _n = 0; // ҳ������
    Span* _prev = nullptr;
    Span* _next = nullptr;

    size_t _objSize=0;
    size_t _useCount = 0; // �кõ�С���ڴ棬�������thread cache�ļ�������ȫ���ջ�ʱ��Ϊ0��ͬʱĬ��Ҳ��0
    bool _inuse = false; // �Ƿ��ڱ�ʹ��
    void* _freeList = nullptr; // �кõ�С���ڴ����������

    size_t _maxSize = 1;
};
class SpanList // ��ͷ˫������
{
private:
    Span* _head = nullptr;
    std::mutex _mutex; // Ͱ��

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
        // prev newspan pos
        prev->_next = newspan;
        newspan->_prev = prev;
        newspan->_next = pos;
        pos->_prev = newspan;
    }
    Span* PopFront()
    {
        Span* front = _head->_next;
        Erase(front); // ֻ�ǽ�frontȡ�����ˣ���û�н��ڴ��ͷ�
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
#include"ObjectPool.h"
#include"PageMap.h"