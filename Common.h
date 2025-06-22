#pragma once
#include<iostream>
#include<vector>
#include<ctime>
#include<cassert>
const static int MAX_BYTES = 256 * 1024;//thread_cache���256KB
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
//�����ڴ�����ӳ��

class SizeClass {
private:
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
    static inline size_t _Index(size_t bytes, size_t align_shift)
    {
        return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
    }

public:
    // ������������10%���ҵ�����Ƭ�˷�
    // [1,128]              8byte���� freelist[0,16)
    // [128+1,1024]         16byte���� freelist[16,72)
    // [1024+1,8*1024]      128byte���� freelist[72,128)
    // [8*1024+1,64*1024]   1024byte���� freelist[128,184)
    // [64*1024+1,256*1024] 8*1024byte���� freelist[184,208)
    //����������������Ĳ��㵽��Ӧ����������������
    //�磺��Ҫ����129�ֽڵ��ڴ棬���ڲ���16�ֽڵ�����������˼���15�ֽڲ��㵽144�������15�ֽھ����˷ѵ�
    
    static inline size_t RoundUp(size_t size)
    {
        if (size <= 128)
        {
            return _RoundUP(size, 8); // ����8�ֽڶ���
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
            //������һ���ߵ�����˵��������
            assert(false);
            return -1;
        }
    }

    //���ҵ�ǰ�������ĸ���Ͱ
    static inline size_t Index(size_t bytes)
    {
        assert(bytes <= MAX_BYTES);
        // ÿ�������ж��ٸ���
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