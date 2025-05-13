#pragma once

#include <cmath>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "RICachePolicy.h"

namespace RonaldCache
{

template<typename Key, typename Value> class RLfuCache;

template<typename Key, typename Value>
class FreqList
{
private:
    struct Node
    {
        int freq; // Access frequency
        Key key;
        Value value;
        std::shared_ptr<Node> pre; // Previous node
        std::shared_ptr<Node> next;

        Node() 
        : freq(1), pre(nullptr), next(nullptr) {}
        Node(Key key, Value value) 
        : freq(1), key(key), value(value), pre(nullptr), next(nullptr) {}
    };

    using NodePtr = std::shared_ptr<Node>;
    int freq_; // Access frequency
    NodePtr head_; // Dummy head node
    NodePtr tail_; // Dummy tail node

public:
    explicit FreqList(int n) 
     : freq_(n) 
    {
      head_ = std::make_shared<Node>();
      tail_ = std::make_shared<Node>();
      head_->next = tail_;
      tail_->pre = head_;
    }

    bool isEmpty() const
    {
      return head_->next == tail_;
    }

    // Node management method
    void addNode(NodePtr node) 
    {
        if (!node || !head_ || !tail_) 
            return;

        node->pre = tail_->pre;
        node->next = tail_;
        tail_->pre->next = node;
        tail_->pre = node;
    }

    void removeNode(NodePtr node)
    {
        if (!node || !head_ || !tail_)
            return;
        if (!node->pre || !node->next) 
            return;

        node->pre->next = node->next;
        node->next->pre = node->pre;
        node->pre = nullptr;
        node->next = nullptr;
    }

    NodePtr getFirstNode() const { return head_->next; }
    
    friend class RLfuCache<Key, Value>;
    //friend class RArcCache<Key, Value>;
};

template <typename Key, typename Value>
class RLfuCache : public RICachePolicy<Key, Value>
{
public:
    using Node = typename FreqList<Key, Value>::Node;
    using NodePtr = std::shared_ptr<Node>;
    using NodeMap = std::unordered_map<Key, NodePtr>;

    RLfuCache(int capacity, int maxAverageNum = 10)
    : capacity_(capacity), minFreq_(INT8_MAX), maxAverageNum_(maxAverageNum),
      curAverageNum_(0), curTotalNum_(0) 
    {}

    ~RLfuCache() override = default;

    void put(Key key, Value value) override
    {
        if (capacity_ == 0)
            return;

        std::lock_guard<std::mutex> lock(mutex_);
        auto it = nodeMap_.find(key);
        if (it != nodeMap_.end())
        {
            // Reset its value
            it->second->value = value;
            // If found, just adjust it directly, no need to search again in get, but the impact is not significant
            getInternal(it->second, value);
            return;
        }

        putInternal(key, value);
    }

    // value is an output parameter
    bool get(Key key, Value& value) override
    {
      std::lock_guard<std::mutex> lock(mutex_);
      auto it = nodeMap_.find(key);
      if (it != nodeMap_.end())
      {
          getInternal(it->second, value);
          return true;
      }

      return false;
    }

    Value get(Key key) override
    {
      Value value;
      get(key, value);
      return value;
    }

    // Clear cache and reclaim resources
    void purge()
    {
      nodeMap_.clear();
      freqToFreqList_.clear();
    }

private:
    void putInternal(Key key, Value value); // Add to cache
    void getInternal(NodePtr node, Value& value); // Get from cache

    void kickOut(); // Remove expired data from cache

    void removeFromFreqList(NodePtr node); // Remove node from frequency list
    void addToFreqList(NodePtr node); // Add to frequency list

    void addFreqNum(); // Increase average access frequency
    void decreaseFreqNum(int num); // Decrease average access frequency
    void handleOverMaxAverageNum(); // Handle case when current average access frequency exceeds limit
    void updateMinFreq();

private:
    int                                            capacity_; // Cache capacity
    int                                            minFreq_; // Minimum access frequency (used to find node with minimum access frequency)
    int                                            maxAverageNum_; // Maximum average access frequency
    int                                            curAverageNum_; // Current average access frequency
    int                                            curTotalNum_; // Total number of accesses to all cache
    std::mutex                                     mutex_; // Mutex for synchronization
    NodeMap                                        nodeMap_; // Mapping from key to cache node
    std::unordered_map<int, FreqList<Key, Value>*> freqToFreqList_; // Mapping from access frequency to frequency list
};

template<typename Key, typename Value>
void RLfuCache<Key, Value>::getInternal(NodePtr node, Value& value)
{
    // After finding it, remove it from the low-frequency list and add it to the list with frequency +1,
    // Access frequency +1, then return the value
    value = node->value;
    // Remove node from the original frequency list
    removeFromFreqList(node); 
    node->freq++;
    addToFreqList(node);
    // If the current node's access frequency equals minFreq+1 and its predecessor list is empty, then
    // the freqToFreqList_[node->freq - 1] list is empty due to node migration, so the minimum access frequency needs to be updated
    if (node->freq - 1 == minFreq_ && freqToFreqList_[node->freq - 1]->isEmpty())
        minFreq_++;

    // The total access frequency and the current average access frequency both increase accordingly
    addFreqNum();
}

template<typename Key, typename Value>
void RLfuCache<Key, Value>::putInternal(Key key, Value value)
{   
    // If not in the cache, need to check whether the cache is full
    if (nodeMap_.size() == capacity_)
    {
        // If the cache is full, delete the least frequently used node and update the current average and total access frequency
        kickOut();
    }
    
    // Create a new node, add it, and update the minimum access frequency
    NodePtr node = std::make_shared<Node>(key, value);
    nodeMap_[key] = node;
    addToFreqList(node);
    addFreqNum();
    minFreq_ = std::min(minFreq_, 1);
}

template<typename Key, typename Value>
void RLfuCache<Key, Value>::kickOut()
{
    NodePtr node = freqToFreqList_[minFreq_]->getFirstNode();
    removeFromFreqList(node);
    nodeMap_.erase(node->key);
    decreaseFreqNum(node->freq);
}

template<typename Key, typename Value>
void RLfuCache<Key, Value>::removeFromFreqList(NodePtr node)
{
    // Check if node is null
    if (!node) 
        return;
    
    auto freq = node->freq;
    freqToFreqList_[freq]->removeNode(node);
}

template<typename Key, typename Value>
void RLfuCache<Key, Value>::addToFreqList(NodePtr node)
{
    // Check if node is null
    if (!node) 
        return;

    // Add to frequency list
    auto freq = node->freq;
    if (freqToFreqList_.find(node->freq) == freqToFreqList_.end())
    {
        // If it doesn't exist, create it
        freqToFreqList_[node->freq] = new FreqList<Key, Value>(node->freq);
    }

    freqToFreqList_[freq]->addNode(node);
}

template<typename Key, typename Value>
void RLfuCache<Key, Value>::addFreqNum()
{
    curTotalNum_++;
    if (nodeMap_.empty())
        curAverageNum_ = 0;
    else
        curAverageNum_ = curTotalNum_ / nodeMap_.size();

    if (curAverageNum_ > maxAverageNum_)
    {
       handleOverMaxAverageNum();
    }
}

template<typename Key, typename Value>
void RLfuCache<Key, Value>::decreaseFreqNum(int num)
{
    // Decrease average access frequency
    curTotalNum_ -= num;
    if (nodeMap_.empty())
        curAverageNum_ = 0;
    else
        curAverageNum_ = curTotalNum_ / nodeMap_.size();
}

template<typename Key, typename Value>
void RLfuCache<Key, Value>::handleOverMaxAverageNum()
{
    if (nodeMap_.empty())
        return;

    // Current average access frequency already exceeds limit, all node access frequencies - (maxAverageNum_ / 2)
    for (auto it = nodeMap_.begin(); it != nodeMap_.end(); ++it)
    {
        // Check if node is null
        if (!it->second)
            continue;

        NodePtr node = it->second;

        // Remove from current frequency list
        removeFromFreqList(node);

        // Decrease frequency
        node->freq -= maxAverageNum_ / 2;
        if (node->freq < 1) node->freq = 1;

        // Add to new frequency list
        addToFreqList(node);
    }

    // Update minimum frequency
    updateMinFreq();
}

template<typename Key, typename Value>
void RLfuCache<Key, Value>::updateMinFreq() 
{
    minFreq_ = INT8_MAX;
    for (const auto& pair : freqToFreqList_) 
    {
        if (pair.second && !pair.second->isEmpty()) 
        {
            minFreq_ = std::min(minFreq_, pair.first);
        }
    }
    if (minFreq_ == INT8_MAX) 
        minFreq_ = 1;
}

// The total access frequency and the current average access frequency both increase accordingly
// It does not sacrifice space for time, but shards the original cache size.
template<typename Key, typename Value>
class RHashLfuCache
{
public:
    RHashLfuCache(size_t capacity, int sliceNum, int maxAverageNum = 10)
        : sliceNum_(sliceNum > 0 ? sliceNum : std::thread::hardware_concurrency())
        , capacity_(capacity)
    {
        size_t sliceSize = std::ceil(capacity_ / static_cast<double>(sliceNum_)); // Capacity of each lfu shard
        for (int i = 0; i < sliceNum_; ++i)
        {
            lfuSliceCaches_.emplace_back(new RLfuCache<Key, Value>(sliceSize, maxAverageNum));
        }
    }

    void put(Key key, Value value)
    {
        // Find the corresponding lfu shard according to the key
        size_t sliceIndex = Hash(key) % sliceNum_;
        return lfuSliceCaches_[sliceIndex]->put(key, value);
    }

    bool get(Key key, Value& value)
    {
        // Find the corresponding lfu shard according to the key
        size_t sliceIndex = Hash(key) % sliceNum_;
        return lfuSliceCaches_[sliceIndex]->get(key, value);
    }

    Value get(Key key)
    {
        Value value;
        get(key, value);
        return value;
    }

    // Clear cache
    void purge()
    {
        for (auto& lfuSliceCache : lfuSliceCaches_)
        {
            lfuSliceCache->purge();
        }
    }

private:
    // Calculate the corresponding hash value for the key
    size_t Hash(Key key)
    {
        std::hash<Key> hashFunc;
        return hashFunc(key);
    }

private:
    size_t capacity_; // Total cache capacity
    int sliceNum_; // Number of cache shards
    std::vector<std::unique_ptr<RLfuCache<Key, Value>>> lfuSliceCaches_; // Container for lfu cache shards
};

} // namespace RonaldCache
