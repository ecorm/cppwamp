/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_LRUCACHE_HPP
#define CPPWAMP_INTERNAL_LRUCACHE_HPP

#include <cassert>
#include <list>
#include <unordered_map>

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename K, typename T, typename H = std::hash<K>,
          typename E = std::equal_to<K>>
class LruCache
{
public:
    using Key = K;
    using Value = T;
    using Hash = H;
    using KeyEqual = E;
    using Size = std::size_t;

    explicit LruCache(Size capacity, Size bucketCount = 1,
                      const Hash& hash = {}, const KeyEqual& equal = {})
        : map_(bucketCount, hash, equal),
          capacity_(capacity)
    {
        assert(capacity_ != 0);
    }

    bool empty() const {return map_.empty();}

    Size size() const {return map_.size();}

    Size capacity() const {return capacity_;}

    float loadFactor() const {return map_.load_factor();}

    float maxLoadFactor() const {return map_.max_load_factor();}

    void setMaxLoadFactor(float mlf) {map_.max_load_factor(mlf);}

    template <typename TKey>
    const Value* lookup(const TKey& key)
    {
        // Search map
        auto found = map_.find(key);
        if (found == map_.end())
            return nullptr;
        Entry& entry = found->second;

        // Update most recently used
        assert(!lruList_.empty());
        if (entry.position != lruList_.begin())
            lruList_.splice(lruList_.begin(), lruList_, entry.position);

        return &(entry.value);
    }

    template <typename TKey>
    void upsert(TKey&& key, Value value)
    {
        auto inserted = map_.emplace(std::forward<TKey>(key), Entry{value});
        if (inserted.second)
        {
            lruList_.push_front(inserted.first->first);
            inserted.first->second.position = lruList_.begin();
        }
        else
        {
            inserted.first->second.value = std::move(value);
        }

        if (map_.size() > capacity_)
            evictLeastRecentlyUsed();
    }

    void clear()
    {
        map_.clear();
        lruList_.clear();
    }

    template <typename P>
    void evictIf(P&& predicate)
    {
        auto iter = map_.begin();
        const auto end = map_.end();
        while (iter != end)
        {
            if (predicate(iter->first, iter->second.value))
            {
                lruList_.erase(iter->second.position);
                iter = map_.erase(iter);
            }
            else
            {
                ++iter;
            }
        }
    }

private:
    using List = std::list<Key>;

    struct Entry
    {
        Entry() = default;
        explicit Entry(Value value) : value(std::move(value)) {}

        Value value = {};
        typename List::iterator position = {};
    };

    void evictLeastRecentlyUsed()
    {
        assert(!lruList_.empty());
        const Key& lruKey = lruList_.back();
        map_.erase(lruKey);
        lruList_.pop_back();
        assert(map_.size() == capacity_);
        assert(lruList_.size() == capacity_);
    }

    std::unordered_map<Key, Entry, H, E> map_;
    List lruList_; // Front is MRU, back is LRU
    Size capacity_ = 0;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_LRUCACHE_HPP
