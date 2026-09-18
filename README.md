# KeeCache

C++17 实现的一组缓存淘汰策略，统一挂在 `CachePolicy` 接口下，便于替换和对比。

包含：

- LRU / LFU（可选 Aging）
- LRU-K
- 哈希分片 LRU / LFU
- ARC（LRU 半边 + LFU 半边 + Ghost）

`main.cpp` 里既有各策略的小例子，也有三场相同随机序列下的命中率对比。

## 依赖与构建

- CMake ≥ 3.10
- 支持 C++17 的编译器（GCC / Clang）

```bash
cmake -S . -B build
cmake --build build
./build/KeeCache
```

可执行文件名是 `KeeCache`，源文件只有 `main.cpp`，策略都在头文件里。

## 目录

```text
KeeCache/
├── CMakeLists.txt
├── CachePolicy.h      抽象接口
├── LruCache.h         LRU
├── LfuCache.h         LFU + 平均频次老化
├── LruKCache.h        第 K 次引用才进主缓存
├── HashLruCache.h     hash(key) 分片后的 LRU
├── HashLfuCache.h     hash(key) 分片后的 LFU
├── ArcLruPart.h       ARC 的最近使用侧 + Ghost
├── ArcLfuPart.h       ARC 的频繁使用侧 + Ghost
├── ArcCache.h         把两半边拼成 ARC
└── main.cpp           功能示例 + 压力对比
```

全部策略在命名空间 `KeeCache` 中。

## 接口

```cpp
template<typename Key, typename Value>
class CachePolicy {
public:
    virtual ~CachePolicy() = default;
    virtual void put(Key key, Value value) = 0;
    virtual bool get(Key key, Value& value) = 0;
    virtual Value get(Key key) = 0;
};
```

- `put`：插入或更新
- `get(key, value)`：命中则写入 `value` 并返回 `true`
- `get(key)`：命中返回值，未命中返回 `Value{}`

拥有堆上节点的类都禁用了拷贝构造和拷贝赋值，避免浅拷贝导致 double-free。

## 策略说明

### LRU（`LruCache`）

双向链表 + 哈希表。命中或更新时把节点移到链表尾（最近使用）；容量满时淘汰链表头（最久未用）。

```cpp
KeeCache::LruCache<int, std::string> cache(2);
cache.put(1, "one");
std::string value;
cache.get(1, value);
```

构造：`LruCache(int capacity)`。

### LFU（`LfuCache`）

按访问频次分组。同频次内仍按 LRU 破平局。淘汰 `minFreq_` 那条链的队头。

可选 Aging：当平均频次超过 `maxAverageNum` 时，所有节点 `freq = max(1, freq / 2)`，减轻「旧热点永远不走」的污染。

```cpp
KeeCache::LfuCache<int, std::string> lfu(2);           // 不老化
KeeCache::LfuCache<int, std::string> aging(2, 3);      // 平均频次 > 3 则衰减
```

构造：`LfuCache(int capacity, int maxAverageNum = INT_MAX)`。

### LRU-K（`LruKCache`）

key 先记在历史计数里，达到 `k` 次才 `admit` 进主 LRU，减轻一次性扫描对主缓存的污染。

构造：`LruKCache(int capacity, int historyCapacity, int k)`。

### 哈希分片（`HashLruCache` / `HashLfuCache`）

总容量切成 `sliceNum` 片，每片一个独立的 LRU 或 LFU。下标：

```text
index = hash(key) % sliceNum
```

片大小向上取整：`(capacity + sliceNum - 1) / sliceNum`。片缓存用 `unique_ptr` 放进 `vector`（底层策略禁拷贝，指针可移动）。`sliceNum <= 0` 时按 `std::thread::hardware_concurrency()` 取片数。

```cpp
KeeCache::HashLruCache<int, std::string> hashLru(1000, 4);
KeeCache::HashLfuCache<int, std::string> hashLfu(1000, 4);
```

### ARC（`ArcCache`）

由 `ArcLruPart`（最近）和 `ArcLfuPart`（频繁）组成：

- 新数据先进入 LRU 侧
- 访问次数达到 `transformThreshold` 后转到 LFU 侧
- 从主缓存淘汰的 key 进入 Ghost（只记「刚被踢过」）
- Ghost 命中会给这一侧加容量、另一侧减容量
- Ghost 记录一次性：`checkGhost` 命中后删除该幽灵节点

```cpp
KeeCache::ArcCache<int, std::string> arc(2, 2);  // 容量, 转正门槛
```

`ArcLruPart` / `ArcLfuPart` 也可单独拿来测半边行为。

## `main.cpp` 里有什么

前半段是各策略的手写小例子（容量 2 的插入、淘汰、幽灵、分片、四线程 Hash-LRU）。

后半段三个压力场景走同一套 `runOnAll`：7 种策略、相同 RNG 种子 `42`、相同操作序列，只比命中率。

| 场景 | 在模拟什么 | 容量 / 操作次数 |
|---|---|---|
| 热点访问 | 70% 打在 20 个热 key，其余打在 5000 个冷 key | 20 / 20 万 |
| 循环扫描 | 扫描圈 500，缓存只有 50 | 50 / 10 万 |
| 工作负载变化 | 热点 → 随机 → 扫描 → 局部 → 混合，共五段 | 30 / 8 万 |

命中率 = 成功 `get` 次数 / `get` 总次数（`put` 不计入分母）。没有一种策略在三个场景里都第一，这是故意的。

## 最小示例

```cpp
#include "LruCache.h"
#include <iostream>
#include <string>

int main() {
    KeeCache::LruCache<int, std::string> cache(2);
    cache.put(1, "one");
    cache.put(2, "two");

    std::string value;
    if (cache.get(1, value)) {
        std::cout << value << "\n";
    }

    cache.put(3, "three");  // 容量满，按 LRU 淘汰最久未用的
}
```

换成 `LfuCache`、`ArcCache` 等时，只要仍走 `CachePolicy` 的 `put` / `get` 即可。

## 实现上的几个约定

- 哨兵节点：链表用 dummy head / dummy tail，避免空链分支
- 三五法则：写了析构并 `new` 了节点的类，拷贝构造 / 拷贝赋值 `= delete`
- `unique_ptr`：分片缓存放进 `vector` 时只搬指针，不搬不能拷贝的 `LruCache` / `LfuCache`
- 整数向上取整：`(a + b - 1) / b`，避免 `a / b` 截断后总容量变少
- Hash 分片本身不是新的淘汰算法，只是把多个小缓存并排，用哈希选片
