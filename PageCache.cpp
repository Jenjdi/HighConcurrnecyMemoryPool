#include"PageCache.h"
PageCache PageCache::_sInst;

//��ȡһ��kҳ��span
Span* PageCache::NewSpan(size_t k)
{
    assert(k > 0 && k < NPAGES);
    //����k��Ͱ���Ƿ���span�������ֱ�ӷŻ�
    if (!_slist[k].Empty())
    {
        return _slist[k].PopFront();
    }
    //��k��Ͱû���ڴ棬���α��������Ͱ���ҵ���span��
    for (size_t i = k + 1;i < NPAGES;i++)
    {
        if (!_slist[i].Empty())
        {
            //�ҵ����õ�span�������е�k��ҳ�з֣�ʣ�µ�i-k��ҳ�ŵ���i-kλ�õ�Ͱ��
            Span* nSpan = _slist[i].PopFront();
            //���õ����span
            Span* kSpan = new Span;
            kSpan->_pageid = nSpan->_pageid;//��spanǰ���k��ҳ�з��������µ�id=ԭ����id
            kSpan->_n = k;//�õ�k��ҳ

            nSpan->_pageid += k;//spanǰ���kҳ���з��ˣ��µ�id��Ҫ����k
            nSpan->_n -= k;
            _slist[nSpan->_n].PushFront(nSpan);
            //����pageid��span��ӳ�䣬����central cache���գ�һ��span���ܶ�Ӧ���ҳ
            for (PAGE_ID i = 0; i < kSpan->_n;i++)
            {
                _idSpanMap[kSpan->_pageid + i] = kSpan;
            }
            return kSpan;
        }
    }
    //�ߵ�����˵���Ѿ�û�д�ҳspan��
    //ȥ��������NPAGES-1ҳ��span
    Span* bigSpan = new Span;
    void* ptr = SystemAlloc(NPAGES - 1);
    //��ȡҳ��
    bigSpan->_pageid = (PAGE_ID)ptr >> PAGE_SHIFT; // �൱�ڳ���ҳ�Ĵ�С���õ�ҳ��
    bigSpan->_n = NPAGES - 1;//�������NPAGES-1ҳ��span

    _slist[bigSpan->_n].PushFront(bigSpan);

    return NewSpan(k);//���������߼�������ʱ��Ϊ�Ѿ�����span�����һ��������������Ի�������
}
Span* PageCache::MapObjectToSpan(void* obj)
{
    PAGE_ID id = (PAGE_ID)obj >> PAGE_SHIFT;
    auto ret = _idSpanMap.find(id);
    if (ret != _idSpanMap.end())
    {
        return ret->second;
    }
    else
    {
        assert(false);//��Ϊ֮ǰ�Ѿ��ŵ���map���ˣ����һ�����ҵ������Ҳ���˵����������
        return nullptr;
    }

}
void PageCache::ReleaseSpanToPageCache(Span* span)
{

}