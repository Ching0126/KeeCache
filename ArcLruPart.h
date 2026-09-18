#pragma once

#include<unordered_map>

namespace KeeCache{

template <typename Key,typename Value>
class ArcLruPart{
private:
    struct Node{
        Key key{};
        Value value{};
        int accessCount = 1;
        Node* prev = nullptr;
        Node* next = nullptr;

        Node() = default;
        Node(Key k,Value v)
            :key(k),
            value(v),
            accessCount(1)//结点的访问次数
            {}

    };

    int capacity_;
    int ghostCapacity_;
    int transformThresHold_;//转化阈值 点击超过阈值数则转到lru缓存
    std::unordered_map<Key,Node*> mainMap_;
    std::unordered_map<Key,Node*> ghostMap_;
    Node* mainHead_;
    Node* mainTail_;
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

    void move2Tail(Node* dummyTail,Node* node){
        unlink(node);
        insert2Tail(dummyTail,node);
    }

    void evictOldestGhost(){
        Node* victim = ghostHead_->next;
        if(victim == ghostTail_){
            return ;
        }
        unlink(victim);
        ghostMap_.erase(victim->key);
        delete victim;
    }

    void evictLeastRecent(){
        Node* victim = mainHead_->next;
        if(victim == mainTail_){
            return;
        }

        unlink(victim);
        mainMap_.erase(victim->key);
        //幽灵链表如果已满，清理幽灵结点
        if(static_cast<int>(ghostMap_.size())>=ghostCapacity_){
            evictOldestGhost();
        }
        victim->accessCount = 1;//这个作用是什么？
        insert2Tail(ghostTail_,victim);
        ghostMap_[victim->key] = victim;
    }

public:
    ArcLruPart(int capacity,int transformThresHold = 2)
        :capacity_(capacity),
        ghostCapacity_(capacity),
        transformThresHold_(transformThresHold){
            mainHead_ = new Node();
            mainTail_ = new Node();
            mainHead_->next = mainTail_;
            mainTail_->prev = mainHead_;

            ghostHead_ = new Node();
            ghostTail_ = new Node();
            ghostHead_->next = ghostTail_;
            ghostTail_->prev = ghostHead_;
        }
    
    ~ArcLruPart(){
        Node* cur = mainHead_;
        while(cur){
            Node* nxt = cur->next;
            delete cur;
            cur = nxt;
        }
        cur = ghostHead_;
        while(cur){
            Node* nxt = cur->next;
            delete cur;
            cur = nxt;
        }
    }

    ArcLruPart(const ArcLruPart&) = delete;
    ArcLruPart& operator=(const ArcLruPart&) = delete;

    bool put(Key key,Value value,bool& shouldTransform){
        if(capacity_<=0){
            return false;
        }

        auto it = mainMap_.find(key);
        if(it!=mainMap_.end()){
            it->second->value = value;
            move2Tail(mainTail_,it->second);
            it->second->accessCount++;
            shouldTransform = (it->second->accessCount >= transformThresHold_);
            return true;
        }

        if(static_cast<int>(mainMap_.size())>=capacity_){
            evictLeastRecent();
        }
        Node* node = new Node(key,value);
        insert2Tail(mainTail_,node);
        mainMap_[key] = node;
        return true;
    }

    bool get(Key key,Value& value,bool& shouldTransform){
        auto it = mainMap_.find(key);
        if(it==mainMap_.end()){
            return false;
        }
        move2Tail(mainTail_,it->second);
        it->second->accessCount++;
        value = it->second->value;
        shouldTransform = (it->second->accessCount >= transformThresHold_);
        return true;
    }

    bool checkGhost(Key key){
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
            evictLeastRecent();
        }
        --capacity_;
        return true;
    }
};
}