#pragma once 
#include <unordered_map>
#include<mutex>
#include "CachePolicy.h"

namespace KeeCache{
template<typename Key,typename Value>
class LruCache:public CachePolicy<Key,Value>{//public CachePolicy<Key,Value> 这是类型，声明两个参数的值
private:
    struct Node{
        Key key{};
        Value value{};
        Node* prev = nullptr;
        Node* next = nullptr;

        Node() = default;
        Node(Key k,Value v):
            key(k),
            value(v){}
    };
    int capacity_;
    std::unordered_map<Key,Node*> nodeMap_;
    Node* dummyHead_;
    Node* dummyTail_;
    std::mutex mutex_;
    
    void removeNode(Node* node){
        node->prev->next = node->next;
        node->next->prev  = node->prev;
        node->prev = nullptr;
        node->next = nullptr;
    }

    void insert2Tail(Node* node){
        node->prev = dummyTail_->prev;
        node->next = dummyTail_;
        dummyTail_->prev->next = node;
        dummyTail_->prev = node;
    }
    void move2Tail(Node* node){
        removeNode(node);
        insert2Tail(node);
    }
    void evictLeastRecent(){
        Node* victim = dummyHead_->next;
        if(victim == dummyTail_){
            return;
        }
        removeNode(victim);
        nodeMap_.erase(victim->key);
        delete victim;
    }
public:
    explicit LruCache(int capacity):capacity_(capacity){
        //显示声明，防止隐式转换
        //这里没有提供set方法，容量只允许再构造函数中设置，防止容量被随意修改。
        //capacity_ 还是私有变量，更没有办法修改了。
        dummyHead_ = new Node();
        dummyTail_ = new Node();
        dummyHead_->next = dummyTail_;
        dummyTail_->prev = dummyHead_;
    }

    LruCache(const LruCache&) = delete;//禁止拷贝构造函数
    LruCache& operator=(const LruCache&) = delete;//禁止拷贝赋值函数

    ~LruCache()override{
        Node* cur = dummyHead_;
        while(cur){
            Node* next = cur->next;
            delete cur;
            cur = next;
        }
    }
    void put(Key key,Value value)override{
        std::lock_guard<std::mutex> lock(mutex_);
        //存储空间过小
        if(capacity_<=0){
            return;
        }
        //元素已存在在哈希表中
        auto it = nodeMap_.find(key);
        if(it != nodeMap_.end()){
            it->second->value = value;
            move2Tail(it->second);
            return;
        }
        //超出容量限制，调用LRU，清理空间   
        if(static_cast<int>(nodeMap_.size())>=capacity_){
            evictLeastRecent();
        }
        //其余，新建节点，插入到链表尾部，并在哈希表中记录
        Node* node = new Node(key,value);
        insert2Tail(node);
        nodeMap_[key] = node;
    }
    bool get(Key key,Value& value)override{
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = nodeMap_.find(key);
        if(it == nodeMap_.end()){
            return false;
        }
        value = it->second->value;
        move2Tail(it->second);
        return true;
    }
    Value get(Key key)override{
        Value value{};
        get(key, value);
        return value;
        
    }

    void remove(Key key){
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = nodeMap_.find(key);
        if(it == nodeMap_.end()){
            return;
        }
        Node* node = it->second;
        removeNode(node);
        nodeMap_.erase(key);
        delete node;
    }
};
}