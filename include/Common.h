#pragma once // 保证头文件只被编译一次，防止重复定义
#include <cstddef> // 包含标准类型定义，如 size_t
#include <atomic> // 包含原子操作相关的头文件 (虽然在这个代码片段中没有直接使用，但可能在其他部分会用到)
#include <array> // 包含 std::array 容器相关的头文件 (虽然在这个代码片段中没有直接使用，但可能在其他部分会用到)

namespace memoryPool // 定义一个名为 memoryPool 的命名空间，用于组织相关的类和常量
{
// 对齐数和大小定义
constexpr size_t ALIGNMENT = 8; // 定义对齐大小为 8 字节，这意味着分配的内存块地址应该是 8 的倍数
constexpr size_t MAX_BYTES = 256 * 1024; // 定义内存池管理的最大字节数为 256KB
constexpr size_t FREE_LIST_SIZE = MAX_BYTES / ALIGNMENT; // 计算空闲链表的大小，假设每个空闲块最小为 ALIGNMENT 字节

// 内存块头部信息
struct BlockHeader
{
    size_t size; // 存储当前内存块的实际大小
    bool     inUse; // 标记当前内存块是否正在被使用 (true 表示正在使用，false 表示空闲)
    BlockHeader* next; // 指向下一个内存块的指针，用于构建空闲链表或其他链表结构
};

// 大小类管理
class SizeClass
{
public:
    static size_t roundUp(size_t bytes) // 静态方法，用于将给定的字节数向上对齐到 ALIGNMENT 的倍数
    {
        return (bytes + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
        // 位运算实现向上对齐：
        // (bytes + ALIGNMENT - 1) 的作用是当 bytes 不是 ALIGNMENT 的倍数时，加上一些值使其超过最近的 ALIGNMENT 倍数。
        // ~(ALIGNMENT - 1) 会创建一个掩码，该掩码的低位 ALIGNMENT 位都是 0，其余位都是 1 (假设 ALIGNMENT 是 2 的幂)。
        // 进行与运算后，会将 bytes 的低位清零，从而实现向上对齐。
    }

    static size_t getIndex(size_t bytes) // 静态方法，用于根据给定的字节数获取一个索引，可能用于选择不同大小的内存块链表
    {
        // 确保bytes至少为ALIGNMENT，防止计算出负数或不合理的索引
        bytes = std::max(bytes, ALIGNMENT);
        // 向上取整后-1
        return (bytes + ALIGNMENT - 1) / ALIGNMENT - 1;
        // 先向上取整到 ALIGNMENT 的倍数，然后除以 ALIGNMENT 得到块的个数，再减 1 得到索引 (从 0 开始)。
    }
};

} // namespace memoryPool
