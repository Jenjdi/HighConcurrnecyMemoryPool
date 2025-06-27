#pragma once
#include <exception>
#include"Common.h"



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
                _memory = (char*)SystemAlloc((_RemainByte)>>PAGE_SHIFT);
                if (_memory == nullptr) {
                    throw std::bad_alloc();
                }
            }
                obj = (T*)_memory;
                // ���ʵ�ʶ���Ĵ�СС����С�ܷ���Ĵ�С�������ٷ���sizeof(void*)���ֽ�
                size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
                //ʹ�ù�����������ʹ�õģ�ֻ�л���ʱ�ŻὫͷ4���ֽڱ�����һ�����ĵ�ַ
                _memory += objSize;
                _RemainByte -= objSize;
            
        }
        new (obj) T;
        //��λnew(placement new)����ָ����һ��ԭʼ�ڴ��ϵ��ù��캯����������󣬶����Ƕ���������ͨ new �����ȷ����ڴ��ٹ������
        return obj;
    }
    void Delete(T* obj)
    {
        obj->~T();
        //��ʽ�������������������û�н��ռ��ͷŵ�
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
