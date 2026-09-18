#pragma once

#include<mutex>
#include"CachePolicy.h"
#include"ArcLruPart.h"
#include"ArcLfuPart.h"

namespace KeeCache{

template<typename Key,typename Value>
class ArcCache : public CachePolicy<Key,Value>{
private:
    bool checkGhostCaches(Key key){
        if(lruPart_.checkGhost(key)){
            if(lfuPart_.decreaseCapacity()){
                lruPart_.increaseCapacity();
            }
        return true;
        }
        if(lfuPart_.checkGhost(key)){
            if(lruPart_.decreaseCapacity()){
                lfuPart_.increaseCapacity();
            }
            return true;
        }
        return false;
    }

    int capacity_;
    ArcLruPart<Key,Value> lruPart_;
    ArcLfuPart<Key,Value> lfuPart_;
    std::mutex mutex_;
public:
    explicit ArcCache(int capacity,int transformThreshold = 2)
        :capacity_(capacity),
        lruPart_(capacity,transformThreshold),
        lfuPart_(capacity){}

    ArcCache(const ArcCache&) = delete;
    ArcCache& operator=(const ArcCache&) = delete;

    void put(Key key,Value value)override{
        bool shouldTransform = false;
        std::lock_guard<std::mutex> lock(mutex_);
        checkGhostCaches(key);
        bool inLfu = lfuPart_.contain(key);//找一下结点在不在lfu
        lruPart_.put(key,value,shouldTransform);
        if(inLfu || shouldTransform){
            lfuPart_.put(key,value);
        }
    }

    bool get(Key key,Value& value)override{
        std::lock_guard<std::mutex> lock(mutex_);
        checkGhostCaches(key);

        bool shouldTransform = false;
        if(lruPart_.get(key,value,shouldTransform)){
            if(shouldTransform){
                lfuPart_.put(key,value);
            }
            return true;
        }
        return lfuPart_.get(key,value);
    }

    Value get(Key key)override{
        Value value{};
        get(key,value);
        return value;
    }
};
}