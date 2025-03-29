#include "../include/MemoryPool.h" // 包含被测试的内存池类的头文件
#include <iostream> // 用于控制台输入输出
#include <vector> // 使用 std::vector 存储动态数组，用于保存分配的内存指针
#include <chrono> // 使用 std::chrono 库进行时间测量
#include <random> // 用于生成随机数，用于模拟不同大小的内存分配
#include <iomanip> // 用于格式化输出，例如设置精度
#include <thread> // 使用 std::thread 创建和管理线程，用于多线程性能测试

using namespace memoryPool; // 假设 MemoryPool 类位于 Kama_memoryPool 命名空间中，方便直接使用
using namespace std::chrono; // 方便使用 std::chrono 命名空间下的类型，如 high_resolution_clock

// 计时器类
class Timer
{
    high_resolution_clock::time_point start; // 存储计时开始的时间点
public:
    Timer() : start(high_resolution_clock::now()) {} // 构造函数，初始化 start 为当前时间

    double elapsed() // 返回从计时开始到现在的毫秒数
    {
        auto end = high_resolution_clock::now(); // 获取当前时间点
        return duration_cast<microseconds>(end - start).count() / 1000.0; // 计算时间差，转换为微秒，再转换为毫秒
    }
};

// 性能测试类
class PerformanceTest
{
private:
    // 测试统计信息 (当前代码中未使用，但可以用于扩展)
    struct TestStats
    {
        double memPoolTime{0.0}; // 内存池测试花费的时间
        double systemTime{0.0}; // 系统默认分配器 (new/delete) 花费的时间
        size_t totalAllocs{0}; // 总分配次数
        size_t totalBytes{0}; // 总分配字节数
    };

public:
    // 1. 系统预热
    static void warmup()
    {
        std::cout << "Warming up memory systems...\n";
        // 使用 pair 来存储指针和对应的大小，方便后续释放时使用正确的大小
        std::vector<std::pair<void*, size_t>> warmupPtrs;

        // 预热内存池
        for (int i = 0; i < 1000; ++i)
        {
            for (size_t size : {32, 64, 128, 256, 512}) { // 预热时分配一些常用大小的内存块
                void* p = MemoryPool::allocate(size); // 使用内存池分配内存
                warmupPtrs.emplace_back(p, size);  // 存储分配的指针和大小
            }
        }

        // 释放预热内存
        for (const auto& [ptr, size] : warmupPtrs)
        {
            MemoryPool::deallocate(ptr, size);  // 使用实际分配的大小进行释放
        }

        std::cout << "Warmup complete.\n\n";
    }

    // 2. 小对象分配测试
    static void testSmallAllocation()
    {
        constexpr size_t NUM_ALLOCS = 100000; // 定义小对象分配的次数
        constexpr size_t SMALL_SIZE = 32; // 定义小对象的大小为 32 字节

        std::cout << "\nTesting small allocations (" << NUM_ALLOCS << " allocations of "
                  << SMALL_SIZE << " bytes):" << std::endl;

        // 测试内存池
        {
            Timer t; // 创建计时器对象
            std::vector<void*> ptrs; // 存储分配的内存指针
            ptrs.reserve(NUM_ALLOCS); // 预先分配足够的空间，避免 vector 扩容

            for (size_t i = 0; i < NUM_ALLOCS; ++i)
            {
                ptrs.push_back(MemoryPool::allocate(SMALL_SIZE)); // 使用内存池分配小对象

                // 模拟真实使用：部分立即释放
                if (i % 4 == 0) // 每分配 4 次，释放一次
                {
                    MemoryPool::deallocate(ptrs.back(), SMALL_SIZE); // 释放最近分配的内存
                    ptrs.pop_back(); // 从 vector 中移除已释放的指针
                }
            }

            for (void* ptr : ptrs) // 释放剩余所有已分配的内存
            {
                MemoryPool::deallocate(ptr, SMALL_SIZE);
            }

            std::cout << "Memory Pool: " << std::fixed << std::setprecision(3) // 设置输出格式，保留三位小数
                      << t.elapsed() << " ms" << std::endl; // 输出内存池分配和释放花费的时间
        }

        // 测试new/delete
        {
            Timer t; // 创建计时器对象
            std::vector<void*> ptrs; // 存储分配的内存指针
            ptrs.reserve(NUM_ALLOCS); // 预先分配足够的空间

            for (size_t i = 0; i < NUM_ALLOCS; ++i)
            {
                ptrs.push_back(new char[SMALL_SIZE]); // 使用 new 分配小对象

                if (i % 4 == 0) // 每分配 4 次，释放一次
                {
                    delete[] static_cast<char*>(ptrs.back()); // 使用 delete[] 释放内存
                    ptrs.pop_back(); // 从 vector 中移除已释放的指针
                }
            }

            for (void* ptr : ptrs) // 释放剩余所有已分配的内存
            {
                delete[] static_cast<char*>(ptr);
            }

            std::cout << "New/Delete: " << std::fixed << std::setprecision(3)
                      << t.elapsed() << " ms" << std::endl; // 输出 new/delete 分配和释放花费的时间
        }
    }

    // 3. 多线程测试
    static void testMultiThreaded()
    {
        constexpr size_t NUM_THREADS = 4; // 定义测试线程的数量
        constexpr size_t ALLOCS_PER_THREAD = 25000; // 定义每个线程分配的次数
        constexpr size_t MAX_SIZE = 256; // 定义分配的最大字节数

        std::cout << "\nTesting multi-threaded allocations (" << NUM_THREADS
                  << " threads, " << ALLOCS_PER_THREAD << " allocations each):"
                  << std::endl;

        auto threadFunc = [](bool useMemPool) // 定义一个 lambda 函数，作为线程的执行体
        {
            std::random_device rd; // 用于获取真随机数种子
            std::mt19937 gen(rd()); // 使用 Mersenne Twister 算法生成随机数
            std::uniform_int_distribution<> dis(8, MAX_SIZE); // 定义一个均匀分布，生成 8 到 MAX_SIZE 之间的随机数
            std::vector<std::pair<void*, size_t>> ptrs; // 存储每个线程分配的内存指针和大小
            ptrs.reserve(ALLOCS_PER_THREAD); // 预先分配足够的空间

            for (size_t i = 0; i < ALLOCS_PER_THREAD; ++i)
            {
                size_t size = dis(gen); // 生成随机大小
                void* ptr = useMemPool ? MemoryPool::allocate(size) // 根据 useMemPool 参数选择使用内存池或 new
                                        : new char[size];
                ptrs.push_back({ptr, size}); // 存储分配的指针和大小

                // 随机释放一些内存
                if (rand() % 100 < 75) // 75%的概率释放
                {  // 75%的概率释放
                    size_t index = rand() % ptrs.size(); // 随机选择一个已分配的内存块
                    if (useMemPool) {
                        MemoryPool::deallocate(ptrs[index].first, ptrs[index].second); // 使用内存池释放
                    } else {
                        delete[] static_cast<char*>(ptrs[index].first); // 使用 delete[] 释放
                    }
                    ptrs[index] = ptrs.back(); // 将最后一个元素移动到被删除的位置，避免 vector 元素的移动
                    ptrs.pop_back(); // 移除最后一个元素
                }
            }

            // 清理剩余内存
            for (const auto& [ptr, size] : ptrs)
            {
                if (useMemPool)
                {
                    MemoryPool::deallocate(ptr, size); // 使用内存池释放
                }
                else
                {
                    delete[] static_cast<char*>(ptr); // 使用 delete[] 释放
                }
            }
        };

        // 测试内存池
        {
            Timer t; // 创建计时器
            std::vector<std::thread> threads; // 存储创建的线程
            threads.reserve(NUM_THREADS); // 预先分配足够的空间

            for (size_t i = 0; i < NUM_THREADS; ++i)
            {
                threads.emplace_back(threadFunc, true); // 创建并启动线程，使用内存池
            }

            for (auto& thread : threads)
            {
                thread.join(); // 等待所有线程执行完毕
            }

            std::cout << "Memory Pool: " << std::fixed << std::setprecision(3)
                      << t.elapsed() << " ms" << std::endl; // 输出内存池在多线程下的性能
        }

        // 测试new/delete
        {
            Timer t; // 创建计时器
            std::vector<std::thread> threads; // 存储创建的线程
            threads.reserve(NUM_THREADS); // 预先分配足够的空间

            for (size_t i = 0; i < NUM_THREADS; ++i)
            {
                threads.emplace_back(threadFunc, false); // 创建并启动线程，使用 new/delete
            }

            for (auto& thread : threads)
            {
                thread.join(); // 等待所有线程执行完毕
            }

            std::cout << "New/Delete: " << std::fixed << std::setprecision(3)
                      << t.elapsed() << " ms" << std::endl; // 输出 new/delete 在多线程下的性能
        }
    }

    // 4. 混合大小测试
    static void testMixedSizes()
    {
        constexpr size_t NUM_ALLOCS = 50000; // 定义混合大小分配的次数
        const size_t SIZES[] = {16, 32, 64, 128, 256, 512, 1024, 2048}; // 定义一组测试用的内存大小

        std::cout << "\nTesting mixed size allocations (" << NUM_ALLOCS
                  << " allocations):" << std::endl;

        // 测试内存池
        {
            Timer t; // 创建计时器
            std::vector<std::pair<void*, size_t>> ptrs; // 存储分配的内存指针和大小
            ptrs.reserve(NUM_ALLOCS); // 预先分配足够的空间

            for (size_t i = 0; i < NUM_ALLOCS; ++i)
            {
                size_t size = SIZES[rand() % 8]; // 随机选择一个测试用的内存大小
                void* p = MemoryPool::allocate(size); // 使用内存池分配内存
                ptrs.emplace_back(p, size); // 存储分配的指针和大小

                // 批量释放
                if (i % 100 == 0 && !ptrs.empty()) // 每分配 100 次，释放一批内存
                {
                    size_t releaseCount = std::min(ptrs.size(), size_t(20)); // 每次释放最多 20 个内存块
                    for (size_t j = 0; j < releaseCount; ++j)
                    {
                        MemoryPool::deallocate(ptrs.back().first, ptrs.back().second); // 释放最近分配的内存
                        ptrs.pop_back(); // 移除已释放的指针
                    }
                }
            }

            for (const auto& [ptr, size] : ptrs) // 释放剩余所有已分配的内存
            {
                MemoryPool::deallocate(ptr, size);
            }

            std::cout << "Memory Pool: " << std::fixed << std::setprecision(3)
                      << t.elapsed() << " ms" << std::endl; // 输出内存池的性能
        }

        // 测试new/delete
        {
            Timer t; // 创建计时器
            std::vector<std::pair<void*, size_t>> ptrs; // 存储分配的内存指针和大小
            ptrs.reserve(NUM_ALLOCS); // 预先分配足够的空间

            for (size_t i = 0; i < NUM_ALLOCS; ++i)
            {
                size_t size = SIZES[rand() % 8]; // 随机选择一个测试用的内存大小
                void* p = new char[size]; // 使用 new 分配内存
                ptrs.emplace_back(p, size); // 存储分配的指针和大小

                if (i % 100 == 0 && !ptrs.empty()) // 每分配 100 次，释放一批内存
                {
                    size_t releaseCount = std::min(ptrs.size(), size_t(20)); // 每次释放最多 20 个内存块
                    for (size_t j = 0; j < releaseCount; ++j)
                    {
                        delete[] static_cast<char*>(ptrs.back().first); // 使用 delete[] 释放内存
                        ptrs.pop_back(); // 移除已释放的指针
                    }
                }
            }

            for (const auto& [ptr, size] : ptrs) // 释放剩余所有已分配的内存
            {
                delete[] static_cast<char*>(ptr);
            }

            std::cout << "New/Delete: " << std::fixed << std::setprecision(3)
                      << t.elapsed() << " ms" << std::endl; // 输出 new/delete 的性能
        }
    }
};

int main()
{
    std::cout << "Starting performance tests..." << std::endl;

    // 预热系统
    PerformanceTest::warmup();

    // 运行测试
    PerformanceTest::testSmallAllocation(); // 测试小对象分配的性能
    PerformanceTest::testMultiThreaded(); // 测试多线程环境下的性能
    PerformanceTest::testMixedSizes(); // 测试混合大小对象分配的性能

    return 0; // 程序结束
}
