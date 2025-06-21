#pragma once
#include <exception>
#include <iostream>

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
    // linux��brk mmap��
    void* ptr = mmap(nullptr, kpage, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

#endif
    if (ptr == nullptr)
        throw std::bad_alloc();
    return ptr;
}
template <class T>
class ObjectPool {
public:
    T* New()
    {
        T* obj = nullptr;
        // ����������Ŀռ������л��пռ䣬����������ϵĽ��ֱ�ӷ���
        if (_freeList) {
            void* next = *(void**)_freeList;
            obj = (T*)_freeList;
            _freeList = next;
        } else {
            // ��ʣ��ɷ���Ŀռ�Ĵ�С��һ������Ĵ�СС��ʱ�򣬾����·���ռ�
            if (_RemainByte < sizeof(T)) {
                _RemainByte = 128 * 1024;
                _memory = (char*)SystemAlloc(_RemainByte);
                if (_memory == nullptr) {
                    throw std::bad_alloc();
                }
                obj = (T*)_memory;
                // ���ʵ�ʶ���Ĵ�СС����С�ܷ���Ĵ�С�������ٷ���sizeof(void*)���ֽ�
                size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
                _memory += objSize;
                _RemainByte -= objSize;
            }
        }
        obj = new T();
        return obj;
    }
    void Delete(T* obj)
    {
        obj->~T();
        *(void**)(obj) = _freeList; // ������ͷ��������
        // ʹ�ö���ָ������Ϊһ��ָ��һ�������þͻ��ɱ������ͣ�����ָ��Ĵ�С��64��32λ�²�ͬ�������Ҫ��ͬ��ֵ����������
        // ����ʹ�ö���ָ����Ϊǰ4/8���ֽڵĵ�ַָ����һ�����ͷſռ�ĵ�ַ
        _freeList = obj;
    }

private:
    char* _memory = nullptr; // ��ǰ�ܹ����ڴ��С��char�����ܹ�����ȷ�ķ���ռ�
    size_t _RemainByte = 0; // ʣ��ɷ���Ŀռ���ֽ���
    void* _freeList = nullptr; // ���������������������ͷָ��
};
