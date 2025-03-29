#pragma once
#include "Common.h"

// 定义memoryPool命名空间，用于封装内存池相关的类和功能
namespace memoryPool 
{

// 线程本地缓存类
// ThreadCache为每个线程提供一个本地内存缓存，用于快速分配和释放小块内存，
// 通过减少线程间竞争提高内存分配效率。
class ThreadCache
{
public:
    // 获取ThreadCache的单例实例
    // 使用thread_local关键字确保每个线程拥有独立的ThreadCache实例，
    // 静态局部变量保证线程安全的初始化。
    static ThreadCache* getInstance()
    {
        static thread_local ThreadCache instance;
        return &instance;
    }

    // 分配指定大小的内存块
    // 参数 size: 请求的内存块大小（以字节为单位）
    // 返回值: 指向分配的内存块的指针
    void* allocate(size_t size);

    // 释放指定的内存块
    // 参数 ptr: 要释放的内存块的起始地址
    // 参数 size: 内存块的大小（以字节为单位）
    void deallocate(void* ptr, size_t size);

private:
    // 私有默认构造函数
    // 防止外部直接实例化ThreadCache，只能通过getInstance()获取实例
    ThreadCache() = default;

    // 从中心缓存获取内存块
    // 参数 index: 自由链表的索引，表示请求的内存块大小类别
    // 返回值: 从中心缓存获取的内存块的指针
    void* fetchFromCentralCache(size_t index);

    // 将内存块归还给中心缓存
    // 参数 start: 要归还的内存块的起始地址
    // 参数 size: 内存块的大小（以字节为单位）
    void returnToCentralCache(void* start, size_t size);

    // 计算批量获取内存块的数量
    // 参数 size: 请求的内存块大小（以字节为单位）
    // 返回值: 建议从中心缓存批量获取的内存块数量
    size_t getBatchNum(size_t size);

    // 判断是否需要将内存块归还给中心缓存
    // 参数 index: 自由链表的索引，表示内存块大小类别
    // 返回值: true表示需要归还，false表示不需要
    bool shouldReturnToCentralCache(size_t index);

private:
    // 自由链表数组
    // freeList_存储不同大小类别的内存块的自由链表，每个元素是一个链表头指针，
    // FREE_LIST_SIZE定义了支持的内存块大小类别的数量。
    std::array<void*, FREE_LIST_SIZE> freeList_;    

    // 自由链表大小统计数组
    // freeListSize_记录每个自由链表中当前内存块的数量，用于管理内存分配和归还策略。
    std::array<size_t, FREE_LIST_SIZE> freeListSize_; 
};

} // namespace memoryPool
