#pragma once
#include <exception>
#include"Common.h"



template <class T>
class ObjectPool {
public:
    T* New()
    {
        T* obj = nullptr;
        // 如果还回来的空间链表中还有空间，则将这个链表上的结点直接分配
        if (_freeList) {
            void* next = *(void**)_freeList;
            obj = (T*)_freeList;
            _freeList = next;
        } else {
            // 当剩余可分配的空间的大小比一个对象的大小小的时候，就重新分配空间
            if (_RemainByte < sizeof(T)) {
                _RemainByte = 128 * 1024;
                _memory = (char*)SystemAlloc((_RemainByte)>>PAGE_SHIFT);
                if (_memory == nullptr) {
                    throw std::bad_alloc();
                }
            }
                obj = (T*)_memory;
                // 如果实际对象的大小小于最小能分配的大小，则最少分配sizeof(void*)个字节
                size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
                //使用过程中是正常使用的，只有回收时才会将头4个字节保存下一个结点的地址
                _memory += objSize;
                _RemainByte -= objSize;
            
        }
        new (obj) T;
        //定位new(placement new)，在指定的一块原始内存上调用构造函数来构造对象，而不是而不是像普通 new 那样先分配内存再构造对象
        return obj;
    }
    void Delete(T* obj)
    {
        obj->~T();
        //显式调用析构函数清理对象，没有将空间释放掉
        *(void**)(obj) = _freeList; // 将对象头插入链表
        // 使用二级指针是因为一级指针一旦解引用就会变成变量类型，由于指针的大小在64和32位下不同，因此需要不同的值来进行设置
        // 所以使用二级指针作为前4/8个字节的地址指向下一个被释放空间的地址
        _freeList = obj;
    }

private:
    char* _memory = nullptr; // 当前总共的内存大小，char类型能够更精确的分配空间
    size_t _RemainByte = 0; // 剩余可分配的空间的字节数
    void* _freeList = nullptr; // 还回来过程中自由链表的头指针
};
