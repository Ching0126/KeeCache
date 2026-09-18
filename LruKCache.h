#pragma once

#include<unordered_map>
#include<cstddef>
#include "CachePolicy.h"
#include "LruCache.h"
#include<mutex>
namespace KeeCache{

template<typename Key,typename Value>
class LruKCache:public CachePolicy<Key,Value>{
private:
    void admit(Key key,Value value){
        history_.remove(key);
        pending_.erase(key);
        mainCache_.put(key,value);
    }
    int k_;
    LruCache<Key,Value> mainCache_;
    LruCache<Key,size_t> history_;
    std::unordered_map<Key,Value> pending_;
    std::mutex mutex_;
public:
    LruKCache(int capacity,int historyCapacity,int k):
        k_(k),
        mainCache_(capacity),
        history_(historyCapacity)
    {}

    LruKCache(const LruKCache&) = delete;
    LruKCache& operator=(const LruKCache&) = delete;

    void put(Key key,Value value)override{
        std::lock_guard<std::mutex> lock(mutex_);
        //存在于主缓存中，直接更新
        Value ignored{};
        if(mainCache_.get(key,ignored)){
            mainCache_.put(key,value);
            return;
        }
        //更新历史缓存，并将其放入待定缓存中
        size_t count = history_.get(key);
        count++;
        history_.put(key,count);
        pending_[key] = value;

        //如果访问次数超过阈值，将其放入主缓存中
        if(count>= static_cast<size_t>(k_)){
                admit(key,value);   
        }
    }

    bool get(Key key,Value& value)override{
        std::lock_guard<std::mutex> lock(mutex_);
        if(mainCache_.get(key,value)){
            return true;
        }
        size_t count = history_.get(key);
        count++;
        history_.put(key,count);
        if(count>= static_cast<size_t>(k_)){
            auto it = pending_.find(key);
            if(it != pending_.end()){
                value = it->second;
                admit(key,value);
                return true;
            }
        }
        return false;
    }

    Value get(Key key)override{
        Value value{};
        get(key, value);
        return value;
    }
};
}