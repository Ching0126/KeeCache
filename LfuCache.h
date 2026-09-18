#pragma once
#include <unordered_map>
#include "CachePolicy.h"
#include<climits>
#include<algorithm>
#include<mutex>
namespace KeeCache{
template<typename Key,typename Value>
class LfuCache:public CachePolicy<Key,Value>{

private:
    struct Node{
        Key key{};
        Value value{};
        int freq = 1;
        Node* prev = nullptr;
        Node* next = nullptr;

        Node() = default;
        Node(Key k,Value v):
            key(k),
            value(v),
            freq(1){}
        
    };

    struct FreqList{
        Node* dummyHead = nullptr;
        Node* dummyTail = nullptr;

        FreqList(){
            dummyHead = new Node();
            dummyTail = new Node();
            dummyHead->next = dummyTail;
            dummyTail->prev = dummyHead;
        }
        ~FreqList(){
            //数据结点由LfuCache析构函数删除，这里只需要删除哨兵。
            delete dummyHead;
            delete dummyTail;
        }
        bool empty()const{
            return dummyHead->next == dummyTail;
        }
        void add2Tail(Node* node){
            node->prev = dummyTail->prev;
            node->next = dummyTail;
            dummyTail->prev->next = node;
            dummyTail->prev = node;
        }
        void remove(Node* node){
            node->prev->next = node->next;
            node->next->prev = node->prev;
            node->prev = nullptr;
            node->next = nullptr;
        }
        Node* front()const{
            return dummyHead->next;
        }
    };

    int capacity_;
    int minFreq_ = 1;
    std::unordered_map<Key,Node*> nodeMap_;//存储key和节点的映射，节点中包含了频次信息。
    std::unordered_map<int,FreqList*> freqMap_;//频次和频次链表的映射，频次链表中存储了所有具有相同频次的节点。
    std::mutex mutex_;
    
    //lfu-aging参数
    int maxAverageNum_;//最大平均访问次数
    int curTotalNum_ = 0;//当前总访问次数

    FreqList* getList(int freq){
        auto it = freqMap_.find(freq);
        if(it == freqMap_.end()){
            //找不到返回一个新建的空的链表。
            freqMap_[freq] = new FreqList();
            return freqMap_[freq];
        }
        return it->second;
    }

    void add2FreqList(Node* node){
        getList(node->freq)->add2Tail(node);
    }

    void removeFromFreqList(Node* node){
        auto it = freqMap_.find(node->freq);
        if(it == freqMap_.end()){
            return;
        }
        it->second->remove(node);//List中的成员函数，返回的类直接访问其函数
    }

    void updateMinFreq(){//Lfu-aging
        minFreq_ = 0;
        for(auto& pair:freqMap_){
            if(!pair.second->empty()){
                if(minFreq_ == 0 || pair.first < minFreq_){
                    minFreq_ = pair.first;
                }
            }
        }
        if(minFreq_ == 0){
            minFreq_ = 1;
        }
    }

    void decay(){//Lfu-aging
        for(auto& pair:nodeMap_){
            Node* node = pair.second;
            removeFromFreqList(node);
            int oldFreq = node->freq;
            int newFreq = std::max(1, node->freq / 2);
            curTotalNum_ -= (oldFreq - newFreq);
            node->freq = newFreq;
            add2FreqList(node);
        }
        updateMinFreq();
    }
    
    void addFreqNum(){//lru-aging
        curTotalNum_++;
        if(nodeMap_.empty()){
            return;
        }
        int avg = curTotalNum_ / static_cast<int>(nodeMap_.size());
        if(avg > maxAverageNum_){
            decay();
        }
    }

    void touch(Node* node){
        int oldFreq = node->freq;
        removeFromFreqList(node);
        node->freq++;
        add2FreqList(node);
        //更新最低频次，可以在缓存队列满了之后，直接删除最低频次的元素。
        if(freqMap_[oldFreq]->empty()&&oldFreq == minFreq_)
            minFreq_++;
        
        addFreqNum();//增加访问次数 lru-aging
    }

    void evictLeastFrequent(){
        auto it = freqMap_.find(minFreq_);
        if(it == freqMap_.end()||it->second->empty()){
            return;
        }
        Node* victim = it->second->front();//获取频次链表的头结点，即最久未使用的节点。
        it->second->remove(victim);//从频次链表中删除该节点
        nodeMap_.erase(victim->key);//从哈希表中删除该节点
        curTotalNum_ -= victim->freq;
        if(curTotalNum_<0) curTotalNum_=0;                                                                                                                      
        delete victim;//释放内存
    }
public:                                                                                                                                                                  
    explicit LfuCache(int capacity,int maxAverageNum=INT_MAX):
    capacity_(capacity),
    maxAverageNum_(maxAverageNum)
   {}

    ~LfuCache()override{
        for(auto& pair:nodeMap_){
            delete pair.second;
        }
        for(auto& pair:freqMap_){
            delete pair.second;
        }
    }

    LfuCache(const LfuCache&) = delete;//禁止拷贝构造函数
    LfuCache& operator=(const LfuCache&) = delete;//禁止拷贝

    void put(Key key,Value value)override{
        std::lock_guard<std::mutex> lock(mutex_);
        if(capacity_ <= 0){
            return;
        }
        auto it = nodeMap_.find(key);
        if(it != nodeMap_.end()){
            it->second->value = value;
            touch(it->second);
            return;
        }
        if(static_cast<int>(nodeMap_.size()) >= capacity_){
            evictLeastFrequent();
        }
        Node* newNode = new Node(key,value);
        nodeMap_[key] = newNode;
        add2FreqList(newNode);
        minFreq_ = 1;
    }

    bool get(Key key,Value& value)override{
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = nodeMap_.find(key);
        if(it == nodeMap_.end()){
            return false;
        }
        value = it->second->value;
        touch(it->second);
        return true;
    }

    Value get(Key key)override{
        Value value{};
        get(key, value);
        return value;
    }

};
}