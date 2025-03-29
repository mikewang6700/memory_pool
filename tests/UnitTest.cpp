#include "../include/MemoryPool.h" // 包含被测试的内存池类的头文件
#include <iostream> // 用于控制台输入输出
#include <vector> // 使用 std::vector 存储动态数组
#include <thread> // 使用 std::thread 创建和管理线程，用于多线程测试
#include <cassert> // 使用 assert 宏进行断言，用于检查程序中的条件是否为真
#include <cstring> // 包含 C 风格的字符串操作函数 (虽然在这个代码片段中没有直接使用，但可能在其他地方用到)
#include <random> // 用于生成随机数，用于压力测试和多线程测试
#include <algorithm> // 包含 std::shuffle 等算法，用于打乱容器中的元素
#include <atomic> // 使用 std::atomic 进行原子操作，用于多线程测试中的错误标记

using namespace memoryPool; // 假设 MemoryPool 类位于 memoryPool 命名空间中，方便直接使用 MemoryPool 而无需加上命名空间前缀

void testBasicAllocation()
{
    std::cout << "Running basic allocation test..." << std::endl;

    // 测试小内存分配
    void* ptr1 = MemoryPool::allocate(8); // 分配 8 字节的内存
    assert(ptr1 != nullptr); // 断言分配成功，返回的指针不为空
    MemoryPool::deallocate(ptr1, 8); // 释放之前分配的 8 字节内存

    // 测试中等大小内存分配
    void* ptr2 = MemoryPool::allocate(1024); // 分配 1024 字节的内存
    assert(ptr2 != nullptr); // 断言分配成功
    MemoryPool::deallocate(ptr2, 1024); // 释放之前分配的 1024 字节内存

    // 测试大内存分配（超过MAX_BYTES）
    void* ptr3 = MemoryPool::allocate(1024 * 1024); // 分配 1MB 的内存
    assert(ptr3 != nullptr); // 断言分配成功
    MemoryPool::deallocate(ptr3, 1024 * 1024); // 释放之前分配的 1MB 内存

    std::cout << "Basic allocation test passed!" << std::endl;
}

void testMemoryWriting()
{
    std::cout << "Running memory writing test..." << std::endl;

    // 分配并写入数据
    const size_t size = 128; // 定义要分配的内存大小为 128 字节
    char* ptr = static_cast<char*>(MemoryPool::allocate(size)); // 分配内存并转换为 char* 类型
    assert(ptr != nullptr); // 断言分配成功

    // 写入数据
    for (size_t i = 0; i < size; ++i)
    {
        ptr[i] = static_cast<char>(i % 256); // 向分配的内存写入一些数据
    }

    // 验证数据
    for (size_t i = 0; i < size; ++i)
    {
        assert(ptr[i] == static_cast<char>(i % 256)); // 验证写入的数据是否正确
    }

    MemoryPool::deallocate(ptr, size); // 释放之前分配的内存
    std::cout << "Memory writing test passed!" << std::endl;
}

void testMultiThreading()
{
    std::cout << "Running multi-threading test..." << std::endl;

    const int NUM_THREADS = 4; // 定义测试线程的数量
    const int ALLOCS_PER_THREAD = 1000; // 定义每个线程分配的次数
    std::atomic<bool> has_error{false}; // 使用原子布尔变量标记是否有错误发生

    auto threadFunc = [&has_error]()
    {
        try
        {
            std::vector<std::pair<void*, size_t>> allocations; // 存储每个线程分配的内存指针和大小
            allocations.reserve(ALLOCS_PER_THREAD); // 预留空间

            for (int i = 0; i < ALLOCS_PER_THREAD && !has_error; ++i)
            {
                size_t size = (rand() % 256 + 1) * 8; // 生成随机的分配大小 (8 的倍数)
                void* ptr = MemoryPool::allocate(size); // 分配内存

                if (!ptr)
                {
                    std::cerr << "Allocation failed for size: " << size << std::endl;
                    has_error = true; // 如果分配失败，设置错误标记
                    break;
                }

                allocations.push_back({ptr, size}); // 将分配的指针和大小添加到 vector 中

                if (rand() % 2 && !allocations.empty()) // 随机地释放一些已经分配的内存
                {
                    size_t index = rand() % allocations.size(); // 随机选择一个已分配的内存块
                    MemoryPool::deallocate(allocations[index].first,
                                            allocations[index].second); // 释放选中的内存块
                    allocations.erase(allocations.begin() + index); // 从 vector 中移除已释放的内存块
                }
            }

            for (const auto& alloc : allocations) // 释放剩余所有已分配但未释放的内存
            {
                MemoryPool::deallocate(alloc.first, alloc.second);
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Thread exception: " << e.what() << std::endl;
            has_error = true; // 如果线程中发生异常，设置错误标记
        }
    };

    std::vector<std::thread> threads; // 存储创建的线程
    for (int i = 0; i < NUM_THREADS; ++i)
    {
        threads.emplace_back(threadFunc); // 创建并启动线程
    }

    for (auto& thread : threads)
    {
        thread.join(); // 等待所有线程执行完毕
    }

    std::cout << "Multi-threading test passed!" << std::endl;
}

void testEdgeCases()
{
    std::cout << "Running edge cases test..." << std::endl;

    // 测试0大小分配
    void* ptr1 = MemoryPool::allocate(0); // 分配 0 字节的内存
    assert(ptr1 != nullptr); // 断言分配成功 (通常内存池会返回一个有效的指针，即使分配大小为 0)
    MemoryPool::deallocate(ptr1, 0); // 释放 0 字节的内存

    // 测试最小对齐大小
    void* ptr2 = MemoryPool::allocate(1); // 分配 1 字节的内存
    assert(ptr2 != nullptr); // 断言分配成功
    assert((reinterpret_cast<uintptr_t>(ptr2) & (ALIGNMENT - 1)) == 0); // 断言分配的内存地址是对齐的 (ALIGNMENT - 1) 的位掩码用于检查最低几位是否为 0
    MemoryPool::deallocate(ptr2, 1); // 释放 1 字节的内存

    // 测试最大大小边界
    void* ptr3 = MemoryPool::allocate(MAX_BYTES); // 分配最大允许大小的内存
    assert(ptr3 != nullptr); // 断言分配成功
    MemoryPool::deallocate(ptr3, MAX_BYTES); // 释放最大允许大小的内存

    // 测试超过最大大小
    void* ptr4 = MemoryPool::allocate(MAX_BYTES + 1); // 分配超过最大允许大小的内存
    assert(ptr4 != nullptr); // 断言分配成功 (根据内存池的实现，可能会返回空指针或者分配一个大于请求的大小)
    MemoryPool::deallocate(ptr4, MAX_BYTES + 1); // 释放超过最大允许大小的内存

    std::cout << "Edge cases test passed!" << std::endl;
}

void testStress()
{
    std::cout << "Running stress test..." << std::endl;

    const int NUM_ITERATIONS = 10000; // 定义压力测试的迭代次数
    std::vector<std::pair<void*, size_t>> allocations; // 存储分配的内存指针和大小
    allocations.reserve(NUM_ITERATIONS); // 预留空间

    for (int i = 0; i < NUM_ITERATIONS; ++i)
    {
        size_t size = (rand() % 1024 + 1) * 8; // 生成随机的分配大小 (8 的倍数)
        void* ptr = MemoryPool::allocate(size); // 分配内存
        assert(ptr != nullptr); // 断言分配成功
        allocations.push_back({ptr, size}); // 将分配的指针和大小添加到 vector 中
    }

    // 随机顺序释放
    std::random_device rd; // 用于获取真随机数种子
    std::mt19937 g(rd()); // 使用 Mersenne Twister 算法生成随机数
    std::shuffle(allocations.begin(), allocations.end(), g); // 打乱 allocations vector 中的元素顺序

    for (const auto& alloc : allocations) // 遍历打乱后的 allocations vector
    {
        MemoryPool::deallocate(alloc.first, alloc.second); // 释放内存，顺序是随机的
    }

    std::cout << "Stress test passed!" << std::endl;
}

int main()
{
    try
    {
        std::cout << "Starting memory pool tests..." << std::endl;

        testBasicAllocation(); // 运行基础分配测试
        testMemoryWriting(); // 运行内存写入测试
        testMultiThreading(); // 运行多线程测试
        testEdgeCases(); // 运行边界测试
        testStress(); // 运行压力测试

        std::cout << "All tests passed successfully!" << std::endl; // 如果所有测试都通过，则打印成功信息
        return 0; // 返回 0 表示程序成功执行
    }
    catch (const std::exception& e) // 捕获可能发生的异常
    {
        std::cerr << "Test failed with exception: " << e.what() << std::endl; // 如果有异常发生，打印错误信息
        return 1; // 返回 1 表示程序执行失败
    }
}

