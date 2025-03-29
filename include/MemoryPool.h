#pragma once // 保证头文件只被编译一次，防止重复定义

#include "ThreadCache.h" // 包含 ThreadCache 类的头文件，该类负责实际的线程本地内存管理

namespace memoryPool // 定义一个名为 memoryPool 的命名空间，用于组织相关的类和函数
{

class MemoryPool // 定义一个名为 MemoryPool 的类，用于提供内存池的功能
{
public:
    static void* allocate(size_t size) // 静态成员函数，用于分配指定大小的内存
    {
        // 调用 ThreadCache 类的单例实例的 allocate 方法来分配内存
        return ThreadCache::getInstance()->allocate(size);
    }

    static void deallocate(void* ptr, size_t size) // 静态成员函数，用于释放之前分配的内存
    {
        // 调用 ThreadCache 类的单例实例的 deallocate 方法来释放内存
        ThreadCache::getInstance()->deallocate(ptr, size);
    }
};

} // namespace memoryPool
