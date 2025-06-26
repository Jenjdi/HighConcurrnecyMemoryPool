#include"PageCache.h"
PageCache PageCache::_sInst;

//获取一个k页的span
Span* PageCache::NewSpan(size_t k)
{
    assert(k > 0 && k < NPAGES);
    //检查第k个桶中是否有span，如果有直接放回
    if (!_slist[k].Empty())
    {
        return _slist[k].PopFront();
    }
    //第k个桶没有内存，依次遍历后面的桶，找到有span的
    for (size_t i = k + 1;i < NPAGES;i++)
    {
        if (!_slist[i].Empty())
        {
            //找到可用的span，将其中的k个页切分，剩下的i-k个页放到第i-k位置的桶中
            Span* nSpan = _slist[i].PopFront();
            //先拿到这个span
            Span* kSpan = new Span;
            kSpan->_pageid = nSpan->_pageid;//将span前面的k个页切分下来，新的id=原来的id
            kSpan->_n = k;//拿到k个页

            nSpan->_pageid += k;//span前面的k页被切分了，新的id需要加上k
            nSpan->_n -= k;
            _slist[nSpan->_n].PushFront(nSpan);
            //建立pageid和span的映射，便于central cache回收，一个span可能对应多个页
            for (PAGE_ID i = 0; i < kSpan->_n;i++)
            {
                _idSpanMap[kSpan->_pageid + i] = kSpan;
            }
            return kSpan;
        }
    }
    //走到这里说明已经没有大页span了
    //去堆上申请NPAGES-1页的span
    Span* bigSpan = new Span;
    void* ptr = SystemAlloc(NPAGES - 1);
    //获取页号
    bigSpan->_pageid = (PAGE_ID)ptr >> PAGE_SHIFT; // 相当于除以页的大小，得到页号
    bigSpan->_n = NPAGES - 1;//申请的是NPAGES-1页的span

    _slist[bigSpan->_n].PushFront(bigSpan);

    return NewSpan(k);//复用整段逻辑，调用时因为已经有了span，因此一定不会走这里，所以会走上面
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
        assert(false);//因为之前已经放到过map中了，因此一定能找到，若找不到说明出问题了
        return nullptr;
    }

}
void PageCache::ReleaseSpanToPageCache(Span* span)
{

}