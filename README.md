# 一个简单的三层缓存结构内存池
---
## 整体介绍
池式组件包括线程池、内存池等。muduo库有很详细的线程池介绍，所以这是学习内存的的一下心得。

1. **ThreadCache(线程本地缓存)**

  - 每个线程独立的内存缓存
  - 无锁操作，快速分配和释放
  - 减少线程间竞争，提高并发性能

2. **CentralCache(中心缓存)**

  - 管理多个线程共享的内存块
  - 通过自旋锁保护，确保线程安全
  - 批量从PageCache获取内存，分配给ThreadCache

3. **PageCache(页缓存)**

  - 从操作系统获取大块内存
  - 将大块内存切分成小块，供CentralCache使用
  - 负责内存的回收和再利用


---
## 设计分析
## 1.ThreadCache层

通过线程本地存储（TLS）减少多线程环境下的锁竞争，提高内存分配和释放的效率，适用于多线程程序中频繁的小块内存分配场景。

**内存分配**:

- 用户调用`allocate(size)`，从本地`freeList_`中查找可用内存块。
- 如果本地缓存不足，调用`fetchFromCentralCache`从中心缓存获取。

**内存释放**:

- 用户调用`deallocate(ptr, size)`，将内存块插入本地`freeList_`。
- 如果本地内存块过多（由`shouldReturnToCentralCache`判断），调用`returnToCentralCache`归还给中心缓存。

**批量优化**: 通过`getBatchNum`计算批量操作数量，减少与中心缓存的频繁交互。



### 1.1 公共接口

1. **`static ThreadCache* getInstance()`返回当前线程的`ThreadCache`实例**

   使用`static thread_local`关键字声明静态局部变量`instance`，确保每个线程拥有独立的`ThreadCache`对象。

   `thread_local`是C++11引入的特性，保证变量的线程局部存储（Thread-Local Storage, TLS）。

   返回指向该实例的指针。

2. **`void* allocate(size_t size)`从线程本地缓存中分配一块指定大小的内存**

   如果`size`为0，调整为`ALIGNMENT`（最小对齐大小）。

   如果`size`超过`MAX_BYTES`，直接用`malloc`从系统分配。

   根据`size`计算自由链表索引`index`（通过`SizeClass::getIndex`）。

   减少`freeListSize_[index]`，表示将分配一个块。

   检查本地自由链表`freeList_[index]`：

   - 如果不为空，取头部内存块并更新链表头，返回该块。
   - 如果为空，调用`fetchFromCentralCache`从中心缓存获取。

3. **`void deallocate(void* ptr, size_t size)` 指定的内存块释放回线程本地缓存**

   如果`size`超过`MAX_BYTES`，直接用`free`释放。

   计算自由链表索引`index`。

   将`ptr`插入`freeList_[index]`头部。

   增加`freeListSize_[index]`。

   检查是否需要归还内存给中心缓存（调用`shouldReturnToCentralCache`）。





### 1.2 私有成员函数

1. **`ThreadCache() = default;`私有默认构造函。

2. **`void* fetchFromCentralCache(size_t index)`**当本地缓存不足时，从中心缓存获取内存块

​	根据`index`计算内存块大小`size`。

​	计算批量数量`batchNum`（通过`getBatchNum`）。

​	从`CentralCache`获取`batchNum`个内存块。

​	更新`freeListSize_[index]`。

​	返回一个内存块，其余存入本地自由链表。

3. **`void returnToCentralCache(void\* start, size_t size)`**将本地缓存中多余的内存块归还给中心缓存

   计算索引`index`和对齐大小`alignedSize`。

​	计算归还数量`returnNum`，保留部分内存（至少1个）。

​	分割自由链表为保留和归还两部分。

​	更新本地链表和大小。

​	调用`CentralCache::returnRange`归还内存。

4. **`size_t getBatchNum(size_t size)`**计算从中心缓存批量获取的内存块数量。

​	根据`size`设置基准批量数`baseNum`。

​	计算最大批量数`maxNum`（不超过4KB）。

​	返回两者较小值，至少为1。

5. **`bool shouldReturnToCentralCache(size_t index)`**判断是否需要将内存块归还给中心缓存。

​	当`freeListSize_[index]`超过阈值（例如64）时返回`true`。





### 1.3 私有成员变量

1. **`std::array<void\*, FREE_LIST_SIZE> freeList_`**
   - **作用**: 存储不同大小类别的自由链表。
   - **细节**:
     - 每个元素是一个指针，指向该大小类别的内存块链表头。
     - `FREE_LIST_SIZE`定义了支持的大小类别数量（可能在`Common.h`中定义）。
   - **意义**: 自由链表是内存池的核心数据结构，用于快速分配和回收内存。
2. **`std::array<size_t, FREE_LIST_SIZE> freeListSize_`**
   - **作用**: 统计每个自由链表中的内存块数量。
   - **细节**: 与`freeList_`对应，记录每个大小类别的可用内存块数。
   - **意义**: 用于管理内存分配策略，例如判断何时从中心缓存获取或归还内存。





---

## 2. CentralCache

`CentralCache` 是内存池设计中的中心缓存层，位于线程缓存（`ThreadCache`）和页缓存（`PageCache`）之间，负责：

1. **内存块分配**: 为线程缓存提供内存块。
2. **内存块回收**: 接收线程缓存归还的内存块并重新分配给其他线程。
3. **线程安全**: 通过自旋锁保护自由链表，确保多线程环境下的安全操作。

### **主要函数分析**

### 2.1 公共接口

1. `static CentralCache& getInstance()`**使用静态局部变量实现线程安全的单例模式**
2. **`fetchRange`**

- **作用**: 从中心缓存获取一批指定大小类别的内存块。
- **参数**:
  - `index`: 自由链表索引，表示内存块大小类别。
  - `batchNum`: 请求的内存块数量。
- **流程**:
  1. 检查输入有效性。
  2. 使用自旋锁保护对 `centralFreeList_[index]` 的访问。
  3. 如果自由链表不为空：
     - 从链表中取 `batchNum` 个块，更新链表头。
  4. 如果自由链表为空：
     - 调用 `fetchFromPageCache` 获取新内存。
     - 将内存切分成小块，构建两部分链表：
       - 一部分（`allocBlocks`）返回给线程缓存。
       - 剩余部分保留在中心缓存。
  5. 释放锁并返回结果。
- **意义**: 批量分配内存块，减少与页缓存的直接交互。

#### 3. **`returnRange`**

- **作用**: 将一批内存块归还到中心缓存。
- **参数**:
  - `start`: 归还链表的起始地址。
  - `size`: 单个内存块大小。
  - `bytes`: 总字节数（实现中未使用，应为 `index`）。
- **流程**:
  1. 检查输入有效性。
  2. 使用自旋锁保护临界区。
  3. 找到归还链表的尾部。
  4. 将归还链表连接到自由链表头部。
  5. 更新自由链表头指针。
  6. 释放锁。
- **意义**: 实现内存重用，提高利用率。



### 2.2 私有成员函数

1. **私有构造函数**

```cpp
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
```

#### 2. **`fetchFromPageCache`**

- **作用**: 从页缓存获取内存。
- **参数**: `size`: 请求的内存块大小。
- **流程**:
  1. 计算需要的页数。
  2. 如果 `size <= 32KB`（`SPAN_PAGES * PageCache::PAGE_SIZE`），分配 8 页。
  3. 否则，按实际需求分配。
- **意义**: 为中心缓存提供新的内存资源。



**2.3 私有成员变量**

```cpp
// 中心缓存的自由链表数组
// 使用原子指针支持线程安全的操作，FREE_LIST_SIZE 定义了大小类别的数量
std::array<std::atomic<void*>, FREE_LIST_SIZE> centralFreeList_;

// 用于同步的自旋锁数组
// 每个自由链表对应一个自旋锁，保护对该链表的并发访问
std::array<std::atomic_flag, FREE_LIST_SIZE> locks_;
};
```

