#pragma once 

#include<unordered_map>

namespace KeeCache{
template<typename Key, typename Value>
class ArcLfuPart{
private:
     struct Node
    {
        Key key{};
        Value value{};
        int freq = 1;
        Node* prev = nullptr;
        Node* next = nullptr;

        Node() = default;
        Node(Key k, Value v)
            : key(k)
            , value(v)
            , freq(1)
        {}
    };

    struct FreqList
    {
        Node* dummyHead = nullptr;
        Node* dummyTail = nullptr;

        FreqList()
        {
            dummyHead = new Node();
            dummyTail = new Node();
            dummyHead->next = dummyTail;
            dummyTail->prev = dummyHead;
        }

        ~FreqList(){
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
    int ghostCapacity_;
    int minFreq_ = 1;
    std::unordered_map<Key,Node*> mainMap_;
    std::unordered_map<Key,Node*> ghostMap_;
    std::unordered_map<int,FreqList*> freqMap_;
    Node* ghostHead_;
    Node* ghostTail_;



    void unlink(Node* node){
        node->prev->next = node->next;
        node->next->prev = node->prev;
        node->prev = nullptr;
        node->next = nullptr;
    }

    void insert2Tail(Node* dummyTail,Node* node){
        node->prev = dummyTail->prev;
        node->next = dummyTail;
        dummyTail->prev->next = node;
        dummyTail->prev = node;
    }

    FreqList* getList(int freq){
        auto it = freqMap_.find(freq);
        if(it == freqMap_.end()){
            freqMap_[freq] = new FreqList();
            return freqMap_[freq];
        }
        return it->second;
    }



    void removeFromFreqList(Node* node){
        auto it = freqMap_.find(node->freq);
        if(it == freqMap_.end()){
            return;
        }
        it->second->remove(node);
    }

    void add2FreqList(Node* node){
        getList(node->freq)->add2Tail(node);
    }

    void updateMinFreq(){
        minFreq_ = 0;
        for(auto&pair : freqMap_){
            if(pair.second && !pair.second->empty()){
                if(minFreq_ ==0 || pair.first < minFreq_){
                    minFreq_ = pair.first;
                }
            }
        }
        if(minFreq_==0){
            minFreq_ = 1;
        }
    }

    void evictOldestGhost(){
        Node* victim = ghostHead_->next;
        if(victim == ghostTail_){
            return;
        }
        unlink(victim);
        ghostMap_.erase(victim->key);
        delete victim;
    }

    void evictLeastFrequent(){
        auto it = freqMap_.find(minFreq_);
        if(it == freqMap_.end() || it->second->empty()){
            return;
        }

        Node* victim = it->second->front();
        it->second->remove(victim);
        mainMap_.erase(victim->key);

        if(static_cast<int>(ghostMap_.size())>=ghostCapacity_){
            evictOldestGhost();
        }
        victim->freq = 1;
        insert2Tail(ghostTail_,victim);
        ghostMap_[victim->key] = victim;

        if(it->second->empty()){
            updateMinFreq();
        }
    }

    void touch(Node* node){
        int oldFreq = node->freq;
        removeFromFreqList(node);
        node->freq++;
        add2FreqList(node);
        if(freqMap_[oldFreq]->empty() && oldFreq == minFreq_){
            minFreq_++;
        }
    }
public:
    explicit ArcLfuPart(int capacity)
        :capacity_(capacity),
        ghostCapacity_(capacity)
    {
        ghostHead_ = new Node();
        ghostTail_ = new Node();
        ghostHead_->next = ghostTail_;
        ghostTail_->prev = ghostHead_;
    }

    ~ArcLfuPart(){
        for(auto& pair:mainMap_){
            delete pair.second;
        }
        for(auto& pair:ghostMap_){
            delete pair.second;
        }
        for(auto& pair : freqMap_){
            delete pair.second;
        }
        delete ghostHead_;
        delete ghostTail_;
    }

    ArcLfuPart(const ArcLfuPart&) = delete;
    ArcLfuPart& operator=(const ArcLfuPart&) = delete;

    bool put(Key key,Value value){
        if(capacity_<=0){
            return false;
        }

        auto it = mainMap_.find(key);
        if(it!=mainMap_.end()){
            it->second->value = value;
            touch(it->second);
            return true;
        }

        if(static_cast<int>(mainMap_.size())>=capacity_){
            evictLeastFrequent();
        }

        Node* node = new Node(key,value);
        mainMap_[key] = node;
        add2FreqList(node);
        minFreq_=1;
        return true;
    }

    bool get(Key key,Value& value){
        auto it = mainMap_.find(key);
        if(it == mainMap_.end()){
            return false;
        }
        value = it->second->value;
        touch(it->second);
        return true;
    }

    bool contain(Key key)const{
        return mainMap_.find(key) != mainMap_.end();
    }

    bool checkGhost(Key key){//找幽灵结点，找到就销毁。
        auto it = ghostMap_.find(key);
        if(it == ghostMap_.end()){
            return false;
        }
        Node* node = it->second;
        unlink(node);
        ghostMap_.erase(it);
        delete node;
        return true;
    }

    void increaseCapacity(){
        ++capacity_;
    }

    bool decreaseCapacity(){
        if(capacity_<=0){
            return false;
        }
        if(static_cast<int>(mainMap_.size()) == capacity_){
            evictLeastFrequent();
        }
        --capacity_;
        return true;
    }
};
}

