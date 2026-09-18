#include<iostream>
#include<string>
#include<thread>
#include<vector>
#include<array>
#include<functional>
#include<iomanip>
#include<random>

#include "LruCache.h"
#include "LfuCache.h"
#include "LruKCache.h"
#include "HashLruCache.h"
#include "HashLfuCache.h"
#include"ArcLruPart.h"
#include"ArcLfuPart.h"
#include"ArcCache.h"

using Policy = KeeCache::CachePolicy<int, std::string>;

static void printResults(const std::string& title,//输出结果的函数
                         int capacity,
                         const std::vector<std::string>& names,
                         const std::vector<int>& hits,//第 i 种策略执行了多少次 get（分母）
                         const std::vector<int>& gets)//其中多少次 get 成功找到了（分子）
{
    std::cout << "=== " << title << " ===\n";
    std::cout << "缓存容量: " << capacity << "\n";
    for (size_t i = 0; i < names.size(); ++i)
    {
        double rate = gets[i] == 0 ? 0.0 : 100.0 * hits[i] / gets[i];
        std::cout << std::left << std::setw(12) << names[i]
                  << " 命中率 " << std::fixed << std::setprecision(2) << rate << "%"
                  << "  (" << hits[i] << "/" << gets[i] << ")\n";
                  //left 左对齐  setw(12)占12 不够补空格
                  //fixed 不要科学计数 2位精度
    }
    std::cout << "\n";
}

static void runOnAll(const std::string& title,//测试标题
                     int capacity,//key容量
                     int operations,//循环多少轮
                     int historyKeys,//只给lru-k的历史表
                     const std::function<int(std::mt19937&, int)>& pickKey,//这一轮访问哪些key的函数
                     int putPercent)//多少Put这一轮
{
    KeeCache::LruCache<int, std::string> lru(capacity);
    KeeCache::LfuCache<int, std::string> lfu(capacity);
                                                                                                                                                                                                                                                                                         
    KeeCache::LruKCache<int, std::string> lruk(capacity, historyKeys, 2);
    KeeCache::LfuCache<int, std::string> aging(capacity, 10000);

    KeeCache::HashLruCache<int, std::string> hashLru(capacity, 4);
    KeeCache::HashLfuCache<int, std::string> hashLfu(capacity, 4);                             

    KeeCache::ArcCache<int, std::string> arc(capacity, 2);                           
                                                          
    std::array<Policy*, 7> caches = {                
        &lru, &lfu, &lruk, &aging, &hashLru, &hashLfu, &arc
    };
    std::vector<std::string> names = {
        "LRU", "LFU", "LRU-K", "LFU-Aging", "Hash-LRU", "Hash-LFU", "ARC"
    };
    std::vector<int> hits(caches.size(), 0);
    std::vector<int> gets(caches.size(), 0);

    const unsigned seed = 42;
    for (size_t i = 0; i < caches.size(); ++i)//跑7个策略
    {
        std::mt19937 gen(seed);
        for (int op = 0; op < operations; ++op)//每个策略跑的次数
        {
            int key = pickKey(gen, op);//每次拿的速记种子都一样
            if (static_cast<int>(gen() % 100) < putPercent)
            {
                caches[i]->put(key, "v" + std::to_string(key));
            }
            else
            {
                std::string out;
                gets[i]++;
                if (caches[i]->get(key, out))
                {
                    hits[i]++;
                }
            }
        }
    }

    printResults(title, capacity, names, hits, gets);
}

static void testHotDataAccess()
{
    const int capacity = 20;
    const int operations = 200000;
    const int hotKeys = 20;
    const int coldKeys = 5000;

    auto pick = [=](std::mt19937& gen, int) {//这是说明书 lambda构造
        if (gen() % 100 < 70)
        {
            return static_cast<int>(gen() % hotKeys);
        }
        return hotKeys + static_cast<int>(gen() % coldKeys);
    };

    runOnAll("场景1：热点访问（70% 打在 20 个热 key 上）",
             capacity, operations, hotKeys + coldKeys, pick, 30);
}

static void testLoopPattern()
{
    const int capacity = 50;
    const int operations = 100000;
    const int loopSize = 500;

    auto pick = [=](std::mt19937& gen, int op) {
        int r = op % 100;
        if (r < 60)
        {
            return op % loopSize;
        }
        if (r < 90)
        {
            return static_cast<int>(gen() % loopSize);
        }
        return loopSize + static_cast<int>(gen() % loopSize);
    };

    runOnAll("场景2：循环扫描（扫描范围 500，容量只有 50）",
             capacity, operations, loopSize * 2, pick, 20);
}

static void testWorkloadShift()
{
    const int capacity = 30;
    const int operations = 80000;
    const int phaseLen = operations / 5;

    auto pick = [=](std::mt19937& gen, int op) {
        int phase = op / phaseLen;
        if (phase == 0)
        {
            return static_cast<int>(gen() % 5);//热点数据
        }
        if (phase == 1)
        {
            return static_cast<int>(gen() % 400);//大范围随机
        }
        if (phase == 2)
        {
            return op % 100;//循环扫描
        }
        if (phase == 3)//局部性
        {
            int locality = (op / 800) % 5;//表明同一段里的800次操作
            return locality * 15 + static_cast<int>(gen() % 15);
        }
        int r = gen() % 100;//混合
        if (r < 40)
        {
            return static_cast<int>(gen() % 5);
        }
        if (r < 70)
        {
            return 5 + static_cast<int>(gen() % 45);
        }
        return 50 + static_cast<int>(gen() % 350);
    };

    runOnAll("场景3：工作负载变化（热点 → 随机 → 扫描 → 局部 → 混合）",
             capacity, operations, 500, pick, 20);
}

int main(){
    std::string value;
    {
        std::cout << "=======Testing LRU Cache=====" << std::endl;
        KeeCache::LruCache<int,std::string> cache(2);
        cache.put(1,"one");
        cache.put(1,"one");
        cache.put(2,"two");
        cache.put(3,"three"); //淘汰1 剩2和3

        std::cout << "Get key 1: " << (cache.get(1,value) ? value : "not found") << std::endl;
        std::cout << "Get key 2: " << (cache.get(2,value) ? value : "not found") << std::endl;
        std::cout << "Get key 3: " << (cache.get(3,value) ? value : "not found") << std::endl;

        cache.put(4,"four");//淘汰2 剩下3和4
        std::cout<< "Get key 3: " << (cache.get(3,value) ? value : "not found") << std::endl;
        std::cout<< "Get key 2: " << (cache.get(2,value) ? value : "not found") << std::endl;
        std::cout<< "Get key 4: " << (cache.get(4,value) ? value : "not found") << std::endl;
    }
    {
        std::cout << "=======Testing LFU Cache=====" << std::endl;
        KeeCache::LfuCache<int,std::string> lfuCache(2);
        lfuCache.put(1,"one");
        lfuCache.put(2,"two");
        lfuCache.get(1,value); //访问1，频次变为2
        lfuCache.get(1,value);//访问1，频次变为3
        lfuCache.get(2,value);//访问2，频次变为2

        lfuCache.put(3,"three");//淘汰2，剩下1和3

        std::cout << "Get key 1: " << (lfuCache.get(1,value) ? value : "not found") << std::endl;
        std::cout << "Get key 2: " << (lfuCache.get(2,value) ? value : "not found") << std::endl;
        std::cout << "Get key 3: " << (lfuCache.get(3,value) ? value : "not found") << std::endl;
    }
    {
    std::cout << "=======Testing LRU-K Cache=====" << std::endl;
     //LRU-k证明一次性的put冲不掉已经进主缓存的数据。
    KeeCache::LruKCache<int,std::string> lruKCache(2,10,2);
    lruKCache.put(1,"one");
    std::cout<<"put once,get(1)"<<(lruKCache.get(1,value) ? value : "not found")<<std::endl;

    KeeCache::LruKCache<int,std::string> lruKCache2(2,20,2);
    lruKCache2.put(2,"hot");
    lruKCache2.put(2,"hot");
    for(int i = 100;i<110;++i){
        lruKCache2.put(i,"cold");
    }
    std::cout<<"after scan,get(2)"<<(lruKCache2.get(2,value) ? value : "not found")<<std::endl;
    std::cout<<"after scan,get(100)"<<(lruKCache2.get(100,value) ? value : "not found")<<std::endl;
    }
   {
        std::cout<<"=====Testing LFU-Aging ====="<<std::endl;
        KeeCache::LfuCache<int,std::string> lfuCache(2);
        lfuCache.put(1,"old-data");
        lfuCache.put(2,"new-data");
        for(int i=0;i<30;i++) lfuCache.get(1,value);
        for(int i=0;i<5;i++) lfuCache.get(2,value);
        lfuCache.put(3,"newest-data");
        std::cout<<"get(1):"<<(lfuCache.get(1,value) ? value : "not found")<<std::endl;
        std::cout<<"get(2):"<<(lfuCache.get(2,value) ? value : "not found")<<std::endl;
        std::cout<<"get(3):"<<(lfuCache.get(3,value) ? value : "not found")<<std::endl;

        KeeCache::LfuCache<int,std::string> aging(2,3);
        aging.put(1,"old-data");
        aging.put(2,"new-data");
        for(int i=0;i<30;i++) aging.get(1,value);
        for(int i=0;i<5;i++) aging.get(2,value);
        aging.put(3,"newest-data");
        std::cout<<"get(1):"<<(aging.get(1,value) ? value : "not found")<<std::endl;
        std::cout<<"get(2):"<<(aging.get(2,value) ? value : "not found")<<std::endl;
        std::cout<<"get(3):"<<(aging.get(3,value) ? value : "not found")<<std::endl;
   }
    {
        std::cout<<"=====Testing Hash_LRU====="<<std::endl;
        KeeCache::HashLruCache<int,std::string> hashlru(4,2);

        hashlru.put(1,"odd-1");
        hashlru.put(3,"odd-3");
        hashlru.put(5,"odd-5");
        std::cout<<"get(1):"<<(hashlru.get(1,value)?value:"notfound")<<std::endl;
        std::cout<<"get(3):"<<(hashlru.get(3,value)?value:"notfound")<<std::endl;
        std::cout<<"get(5):"<<(hashlru.get(5,value)?value:"notfound")<<std::endl;

        hashlru.put(2,"odd-2");
        hashlru.put(4,"odd-4");
        std::cout<<"get(2):"<<(hashlru.get(2,value)?value:"notfound")<<std::endl;
        std::cout<<"get(4):"<<(hashlru.get(4,value)?value:"notfound")<<std::endl;
        std::cout<<"get(3):"<<(hashlru.get(3,value)?value:"notfound")<<std::endl;
    }
    {
        std::cout<<"=====Testing Hash_LFU====="<<std::endl;
        KeeCache::HashLfuCache<int,std::string> hashlfu(4,2);

        hashlfu.put(1,"odd-1");
        hashlfu.put(3,"odd-3");
        hashlfu.put(1,"odd-1");
        hashlfu.put(5,"odd-5");
        std::cout<<"get(1):"<<(hashlfu.get(1,value)?value:"notfound")<<std::endl;
        std::cout<<"get(3):"<<(hashlfu.get(3,value)?value:"notfound")<<std::endl;
        std::cout<<"get(5):"<<(hashlfu.get(5,value)?value:"notfound")<<std::endl;
    }   
    {
        std::cout<<"=====Tesing concurrent Hash_LRU====="<<std::endl;
        KeeCache::HashLruCache<int,std::string> cache(1000,4);
        std::vector<std::thread> threads;
        for(int t=0;t<4;t++){
            threads.emplace_back([&cache,t](){
                for(int i=0;i<500;++i){
                    int key = t*500 + i;
                    cache.put(key,"v");
                    std::string out;
                    cache.get(key,out);
                }
            });
        }
        /*
            lambda的形式：[捕获]（参数）{函数体}
            [捕获] (参数) mutable -> 返回类型 { 函数体 }
            []里面放的是来自外部的变量
        */
        for(auto& th:threads){
            th.join();
        }
        std::string out;
        std::cout<<"get(0)"<<(cache.get(0,out)?out:"not found")<<std::endl;
        std::cout<<"concurrent done"<<std::endl;
    }
    {
        std::cout<<"=====Testing ARC LRU PART====="<<std::endl;
        KeeCache::ArcLruPart<int,std::string> arcLru(2,2);
        bool a=0;
        arcLru.put(1,"one",a);
        arcLru.put(2,"two",a);
        arcLru.put(3,"three",a);

        bool shouldTransform = false;
        std::cout<<"main get(1)"<<(arcLru.get(1,value,shouldTransform)?value:"no found")<<std::endl;
        std::cout<<"ghost has 1："<<(arcLru.checkGhost(1)?"yes":"no found")<<std::endl;
        std::cout<<"ghost has 1 again： "<<(arcLru.checkGhost(1)?"yes":"no found")<<std::endl;

        arcLru.put(1,"one",a);
        std::cout<<"main get(1) after put："<<(arcLru.get(1,value,shouldTransform)?value:"no found")<<std::endl;
        std::cout << "shouldTransform: " << (shouldTransform ? "true" : "false")<< std::endl;
    }
    {
        std::cout<<"=====Testing ARC LFU PART====="<<std::endl;
        KeeCache::ArcLfuPart<int,std::string> arcLfu(2);

        arcLfu.put(1,"one");
        arcLfu.put(2,"two");
        arcLfu.get(1,value);
        arcLfu.get(1,value);
        arcLfu.put(3,"three");

        std::cout<<"main get(1)"<<(arcLfu.get(1,value)?value:"not found")<<std::endl;
        std::cout<<"main get(2)"<<(arcLfu.get(2,value)?value:"not found")<<std::endl;
        std::cout<<"main get(3)"<<(arcLfu.get(3,value)?value:"not found")<<std::endl;
        std::cout<<"ghost has 2:"<<(arcLfu.checkGhost(2)?"yes":"no")<<std::endl;
        std::cout<<"contain(1)"<<(arcLfu.contain(1)?"yes":"no")<<std::endl;
    }
    {
        std::cout<<"=====Testing ARC Cache====="<<std::endl;
        KeeCache::ArcCache<int,std::string> arc(2,2);
        
        arc.put(1,"hot");
        arc.put(2,"cold");
        arc.put(1,"hot");
        arc.put(3,"three");
        arc.put(4,"four");

        std::cout<<"get(1)"<<(arc.get(1,value)?value:"not found")<<std::endl;
        std::cout<<"get(2)"<<(arc.get(2,value)?value:"not found")<<std::endl;
        std::cout<<"get(4)"<<(arc.get(4,value)?value:"not found")<<std::endl;
    }

    std::cout << "\n每种策略看到的访问序列完全相同（种子 42）\n\n";
    testHotDataAccess();
    testLoopPattern();
    testWorkloa dShift();
    return 0;
}