#pragma once
#include "Common.h"
#include <map>
#include <mutex>

namespace memoryPool
{

class PageCache
{
public:
    // 定义页面大小为 4KB (4096 字节)，这是操作系统中常见的页面大小
    static const size_t PAGE_SIZE = 4096;

    // 单例模式：获取 PageCache 的唯一实例
    static PageCache& getInstance()
    {
        static PageCache instance; // 静态局部变量确保只创建一个实例
        return instance;
    }

    // 分配指定页数的内存块（span）
    void* allocateSpan(size_t numPages);

    // 释放之前分配的内存块（span）
    void deallocateSpan(void* ptr, size_t numPages);

private:
    // 私有构造函数，防止外部直接实例化，只能通过 getInstance 获取
    PageCache() = default;

    // 向操作系统申请内存的私有方法
    void* systemAlloc(size_t numPages);

private:
    // Span 结构体：表示一个内存块
    struct Span
    {
        void*  pageAddr; // 内存块的起始地址
        size_t numPages; // 内存块包含的页面数量
        Span*  next;     // 指向下一个 Span 的指针，用于链表管理
    };

    // 空闲 Span 链表的容器，按页面数量组织
    // key 是页面数，value 是对应页面数的空闲 Span 链表头指针
    std::map<size_t, Span*> freeSpans_;

    // 已分配 Span 的映射，用于回收时查找
    // key 是 Span 的起始地址，value 是对应的 Span 对象指针
    std::map<void*, Span*> spanMap_;

    // 互斥锁，用于保护 freeSpans_ 和 spanMap_ 的线程安全访问
    std::mutex mutex_;
};

} // namespace memoryPool
