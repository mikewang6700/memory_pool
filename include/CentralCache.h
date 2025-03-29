#pragma once
#include "Common.h"
#include <mutex>

// 定义 memoryPool 命名空间，用于封装内存池相关的类和功能
namespace memoryPool
{

// 中心缓存类
// CentralCache 是内存池中的共享层，负责管理内存块的分配和回收，
// 为线程缓存提供内存块，并将线程缓存归还的内存块重新分配给其他线程。
class CentralCache
{
public:
    // 获取 CentralCache 的单例实例
    // 使用静态局部变量实现线程安全的单例模式
    static CentralCache& getInstance()
    {
        static CentralCache instance;
        return instance;
    }

    // 从中心缓存获取一批内存块
    // 参数 index: 自由链表的索引，表示请求的内存块大小类别
    // 参数 batchNum: 请求的内存块数量
    // 返回值: 指向获取的内存块链表的指针
    void* fetchRange(size_t index, size_t batchNum);

    // 将一批内存块归还到中心缓存
    // 参数 start: 归还的内存块链表的起始地址
    // 参数 size: 单个内存块的大小（以字节为单位）
    // 参数 bytes: 归还的内存块总字节数（此处未直接使用，可能是为了兼容性保留）
    void returnRange(void* start, size_t size, size_t bytes);

private:
    // 私有构造函数
    // 防止外部直接实例化 CentralCache，只能通过 getInstance() 获取实例
    CentralCache()
    {
        // 初始化中心缓存的自由链表，所有原子指针设为 nullptr
        for (auto& ptr : centralFreeList_)
        {
            ptr.store(nullptr, std::memory_order_relaxed);
        }
        // 初始化所有自旋锁为未锁定状态
        for (auto& lock : locks_)
        {
            lock.clear();
        }
    }

    // 从页缓存获取内存
    // 参数 size: 请求的内存块大小（以字节为单位）
    // 返回值: 指向从页缓存获取的内存的指针
    void* fetchFromPageCache(size_t size);

private:
    // 中心缓存的自由链表数组
    // 使用原子指针支持线程安全的操作，FREE_LIST_SIZE 定义了大小类别的数量
    std::array<std::atomic<void*>, FREE_LIST_SIZE> centralFreeList_;

    // 用于同步的自旋锁数组
    // 每个自由链表对应一个自旋锁，保护对该链表的并发访问
    std::array<std::atomic_flag, FREE_LIST_SIZE> locks_;
};

} // namespace memoryPool

