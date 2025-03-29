#include "PageCache.h"
#include <sys/mman.h> // 用于 mmap 系统调用
#include <cstring>    // 用于 memset

namespace memoryPool
{

// 分配指定页数的内存块（span）
void* PageCache::allocateSpan(size_t numPages)
{
    // 加锁，确保线程安全
    std::lock_guard<std::mutex> lock(mutex_);

    // 在 freeSpans_ 中查找第一个页数大于等于 numPages 的空闲 Span
    auto it = freeSpans_.lower_bound(numPages);
    if (it != freeSpans_.end()) // 找到合适的空闲 Span
    {
        Span* span = it->second; // 获取链表头部的 Span

        // 将该 Span 从空闲链表中移除
        if (span->next)
        {
            freeSpans_[it->first] = span->next; // 更新链表头
        }
        else
        {
            freeSpans_.erase(it); // 如果是最后一个 Span，删除该条目
        }

        // 如果 Span 页数大于请求的 numPages，进行分割
        if (span->numPages > numPages)
        {
            // 创建新 Span 表示多余的部分
            Span* newSpan = new Span;
            newSpan->pageAddr = static_cast<char*>(span->pageAddr) + numPages * PAGE_SIZE; // 计算新 Span 的起始地址
            newSpan->numPages = span->numPages - numPages; // 计算剩余页数
            newSpan->next = nullptr;

            // 将多余部分插入到对应的空闲链表头部
            auto& list = freeSpans_[newSpan->numPages];
            newSpan->next = list;
            list = newSpan;

            // 更新当前 Span 的页数为请求的页数
            span->numPages = numPages;
        }

        // 记录分配的 Span 到 spanMap_，便于回收
        spanMap_[span->pageAddr] = span;
        return span->pageAddr; // 返回内存块地址
    }

    // 没有合适的空闲 Span，向系统申请新内存
    void* memory = systemAlloc(numPages);
    if (!memory) return nullptr; // 分配失败返回空指针

    // 创建新的 Span 对象
    Span* span = new Span;
    span->pageAddr = memory;
    span->numPages = numPages;
    span->next = nullptr;

    // 记录新 Span 到 spanMap_
    spanMap_[memory] = span;
    return memory; // 返回新分配的内存地址
}

// 释放指定的内存块（span）
void PageCache::deallocateSpan(void* ptr, size_t numPages)
{
    // 加锁，确保线程安全
    std::lock_guard<std::mutex> lock(mutex_);

    // 在 spanMap_ 中查找对应的 Span
    auto it = spanMap_.find(ptr);
    if (it == spanMap_.end()) return; // 未找到，说明不是 PageCache 分配的，直接返回

    Span* span = it->second; // 获取对应的 Span 对象

    // 尝试与下一个相邻的 Span 合并
    void* nextAddr = static_cast<char*>(ptr) + numPages * PAGE_SIZE; // 计算下一个 Span 的起始地址
    auto nextIt = spanMap_.find(nextAddr);

    if (nextIt != spanMap_.end()) // 如果下一个地址存在于 spanMap_ 中
    {
        Span* nextSpan = nextIt->second; // 获取相邻的 Span

        // 检查 nextSpan 是否在空闲链表中
        bool found = false;
        auto& nextList = freeSpans_[nextSpan->numPages];

        // 如果 nextSpan 是链表头
        if (nextList == nextSpan)
        {
            nextList = nextSpan->next; // 更新链表头
            found = true;
        }
        else if (nextList) // 如果链表非空，遍历查找
        {
            Span* prev = nextList;
            while (prev->next)
            {
                if (prev->next == nextSpan)
                {
                    prev->next = nextSpan->next; // 从链表中移除 nextSpan
                    found = true;
                    break;
                }
                prev = prev->next;
            }
        }

        // 如果 nextSpan 是空闲的，进行合并
        if (found)
        {
            span->numPages += nextSpan->numPages; // 增加当前 Span 的页数
            spanMap_.erase(nextAddr); // 从 spanMap_ 中移除 nextSpan
            delete nextSpan; // 删除合并的 Span 对象
        }
    }

    // 将当前 Span（可能已合并）插入到空闲链表头部
    auto& list = freeSpans_[span->numPages];
    span->next = list;
    list = span;
}

// 向操作系统申请指定页数的内存
void* PageCache::systemAlloc(size_t numPages)
{
    size_t size = numPages * PAGE_SIZE; // 计算需要的总字节数

    // 使用 mmap 分配匿名私有内存
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) return nullptr; // 分配失败返回空指针

    // 将分配的内存清零
    memset(ptr, 0, size);
    return ptr; // 返回分配的内存地址
}

} // namespace memoryPool
