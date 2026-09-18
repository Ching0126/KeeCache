#pragma once

#include<memory>
#include<thread>
#include<vector>
#include "CachePolicy.h"
#include "LruCache.h"

namespace KeeCache{

template <typename Key,typename Value>
class HashLruCache:public CachePolicy<Key,Value>{
private:
    size_t index(Key key)const{
        return std::hash<Key>{}(key)%static_cast<size_t>(sliceNum_);
        //std::hash<key> 一种类 专门给key坐哈希 这里省略了他的变量名 使用{}充当临时变量。
        //利用临时变量（），对key运算，返回其哈希值。
    }
    int capacity_;//总容量
    int sliceNum_;//切成几片
    std::vector<std::unique_ptr<LruCache<Key,Value>>> slices_;//几篇小的LRU的集合
    //std::vector<LruCache<Key, Value>> slices_;  // 不行，LruCache 不能拷贝 所以面对数据扩容的时候可能会出错。
    //unique_ptr 可以移动、不能拷贝，正好，可以移动。

public:
    HashLruCache(int capacity,int sliceNum)
        :capacity_(capacity),
        sliceNum_(sliceNum>0?sliceNum:static_cast<int>(std::thread::hardware_concurrency())){//默认按照cpu的核数来
            int sliceSize = (capacity_ + sliceNum_ - 1) / sliceNum_;//片内空间
            //capacity_ / sliceNum_属于向下取整
            //这样可以向上取整。确保分片充足。
            for(int i=0;i<sliceNum_;++i){
                slices_.emplace_back(std::make_unique<LruCache<Key,Value>>(sliceSize));
            }
        }

        HashLruCache(const HashLruCache&) = delete;
        HashLruCache& operator=(const HashLruCache&) = delete;

        void put(Key key,Value value)override{
            slices_[index(key)]->put(key,value);
        }

        bool get(Key key,Value& value)override{
            return slices_[index(key)]->get(key,value);
        }

        Value get(Key key)override{
            Value value{};
            get(key,value);
            return value;
        }
};
}