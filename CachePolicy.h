#pragma once //这个头文件在一次编译里只处理一遍。
namespace KeeCache{
template<typename Key,typename Value>//存在模板，即这是一个模板。
class CachePolicy{
    public:
        virtual ~CachePolicy() = default;

        virtual void put(Key key,Value value) = 0;

        virtual bool get(Key key,Value& value) = 0;

        virtual Value get(Key key) = 0;
};
}