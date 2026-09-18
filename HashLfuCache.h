#pragma once

#include<memory>
#include<thread>
#include<vector>
#include<climits>
#include"CachePolicy.h"
#include"LfuCache.h"

namespace KeeCache{

template <typename Key,typename Value>
class HashLfuCache:public CachePolicy<Key,Value>{
private:
    size_t index(Key key)const{
        return std::hash<Key>{}(key)%static_cast<size_t>(sliceNum_);
    }

    int capacity_;
    int sliceNum_;
    std::vector<std::unique_ptr<LfuCache<Key,Value>>> slices_;
public:
    HashLfuCache(int capacity,int sliceNum,int maxAverageNum = INT_MAX)
        :capacity_(capacity),
        sliceNum_(sliceNum>0?sliceNum:static_cast<int>(std::thread::hardware_concurrency())){
            int sliceSize = (capacity_ + sliceNum_ -1)/sliceNum_;
            for(int i=0;i<sliceNum_;i++){
                slices_.emplace_back(std::make_unique<LfuCache<Key,Value>>(sliceSize,maxAverageNum));
            }
        }

    HashLfuCache(const HashLfuCache&) = delete;
    HashLfuCache& operator=(const HashLfuCache&) = delete;

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