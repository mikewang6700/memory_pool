#include "../include/CentralCache.h"
#include "../include/PageCache.h"
#include <cassert>
#include <thread>

// 定义 memoryPool 命名空间
namespace memoryPool
{

// 每次从 PageCache 获取的 span 大小（以页为单位）
static const size_t SPAN_PAGES = 8;

// 从中心缓存获取一批内存块
// 参数 index: 自由链表的索引
// 参数 batchNum: 请求的内存块数量
// 返回值: 指向获取的内存块链表的指针
void* CentralCache::fetchRange(size_t index, size_t batchNum)
{
    // 输入有效性检查：索引超出范围或请求数量为0时返回空
    if (index >= FREE_LIST_SIZE || batchNum == 0) 
        return nullptr;

    // 使用自旋锁保护临界区，确保线程安全
    while (locks_[index].test_and_set(std::memory_order_acquire))
    {
        std::this_thread::yield(); // 线程让步，避免忙等待，降低 CPU 占用
    }

    void* result = nullptr;
    try 
    {
        // 尝试从中心缓存的自由链表获取内存块
        result = centralFreeList_[index].load(std::memory_order_relaxed);

        if (!result) // 如果自由链表为空
        {
            // 根据索引计算内存块大小
            size_t size = (index + 1) * ALIGNMENT;
            // 从页缓存获取新的内存
            result = fetchFromPageCache(size);

            if (!result) // 如果页缓存分配失败
            {
                locks_[index].clear(std::memory_order_release);
                return nullptr;
            }

            // 将从页缓存获取的内存切分成小块
            char* start = static_cast<char*>(result);
            size_t totalBlocks = (SPAN_PAGES * PageCache::PAGE_SIZE) / size; // 总块数
            size_t allocBlocks = std::min(batchNum, totalBlocks); // 分配块数

            // 构建返回给线程缓存的内存块链表
            if (allocBlocks > 1) 
            {  
                for (size_t i = 1; i < allocBlocks; ++i) 
                {
                    void* current = start + (i - 1) * size;
                    void* next = start + i * size;
                    *reinterpret_cast<void**>(current) = next; // 链接内存块
                }
                *reinterpret_cast<void**>(start + (allocBlocks - 1) * size) = nullptr; // 链表尾置空
            }

            // 构建保留在中心缓存的链表
            if (totalBlocks > allocBlocks)
            {
                void* remainStart = start + allocBlocks * size;
                for (size_t i = allocBlocks + 1; i < totalBlocks; ++i)
                {
                    void* current = start + (i - 1) * size;
                    void* next = start + i * size;
                    *reinterpret_cast<void**>(current) = next;
                }
                *reinterpret_cast<void**>(start + (totalBlocks - 1) * size) = nullptr;

                // 更新中心缓存的自由链表
                centralFreeList_[index].store(remainStart, std::memory_order_release);
            }
        } 
        else // 如果自由链表不为空
        {
            // 从现有链表中获取指定数量的块
            void* current = result;
            void* prev = nullptr;
            size_t count = 0;

            while (current && count < batchNum)
            {
                prev = current;
                current = *reinterpret_cast<void**>(current);
                count++;
            }

            if (prev) 
            {
                *reinterpret_cast<void**>(prev) = nullptr; // 截断链表
            }

            // 更新自由链表头指针
            centralFreeList_[index].store(current, std::memory_order_release);
        }
    }
    catch (...) 
    {
        locks_[index].clear(std::memory_order_release); // 异常时释放锁
        throw;
    }

    // 释放自旋锁
    locks_[index].clear(std::memory_order_release);
    return result;
}

// 将一批内存块归还到中心缓存
// 参数 start: 归还的内存块链表起始地址
// 参数 size: 单个内存块大小
// 参数 index: 自由链表索引（代码中未直接使用 bytes 参数，应为 index）
void CentralCache::returnRange(void* start, size_t size, size_t index)
{
    // 输入有效性检查：指针为空或索引无效时直接返回
    if (!start || index >= FREE_LIST_SIZE) 
        return;

    // 使用自旋锁保护临界区
    while (locks_[index].test_and_set(std::memory_order_acquire)) 
    {
        std::this_thread::yield();
    }

    try 
    {
        // 找到归还链表的尾部
        void* end = start;
        size_t count = 1;
        while (*reinterpret_cast<void**>(end) != nullptr && count < size) 
        {
            end = *reinterpret_cast<void**>(end);
            count++;
        }

        // 将归还链表连接到中心缓存自由链表头部
        void* current = centralFreeList_[index].load(std::memory_order_relaxed);
        *reinterpret_cast<void**>(end) = current; // 尾部指向原链表头
        centralFreeList_[index].store(start, std::memory_order_release); // 更新链表头
    }
    catch (...) 
    {
        locks_[index].clear(std::memory_order_release);
        throw;
    }

    locks_[index].clear(std::memory_order_release);
}

// 从页缓存获取内存
// 参数 size: 请求的内存块大小
// 返回值: 指向从页缓存获取的内存的指针
void* CentralCache::fetchFromPageCache(size_t size)
{   
    // 计算需要的页数，向上取整
    size_t numPages = (size + PageCache::PAGE_SIZE - 1) / PageCache::PAGE_SIZE;

    // 根据大小决定分配策略
    if (size <= SPAN_PAGES * PageCache::PAGE_SIZE) 
    {
        // 小于等于 32KB 的请求，分配固定 8 页
        return PageCache::getInstance().allocateSpan(SPAN_PAGES);
    } 
    else 
    {
        // 大于 32KB 的请求，按实际需求分配页数
        return PageCache::getInstance().allocateSpan(numPages);
    }
}

} // namespace memoryPool

