#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include <functional>
#include <algorithm>
#include <mutex>
#include <stdexcept>

/**
 * @class ConsistentHash
 * @brief Class implementing the consistent hashing algorithm.
 *
 * Consistent hashing is a distributed hashing technique designed to minimize key redistribution when nodes are added or removed.
 * Commonly used in distributed cache systems and distributed database sharding.
 */
class ConsistentHash {
public:
    /**
     * @brief Constructor
     * @param numReplicas Number of virtual nodes per physical node. Increasing virtual nodes can improve load balancing.
     * @param hashFunc Optional custom hash function, defaults to std::hash.
     */
    ConsistentHash(size_t numReplicas, std::function<size_t(const std::string&)> hashFunc = std::hash<std::string>())
        : numReplicas_(numReplicas), hashFunction_(hashFunc) {}

    /**
     * @brief Add a node to the hash ring.
     *
     * Each node is replicated into several virtual nodes. Each virtual node calculates a unique hash value using `node + index`.
     * These hash values are stored on the hash ring and sorted for efficient lookup.
     *
     * @param node Name of the node to add (e.g., server address).
     */
    void addNode(const std::string& node) {
        std::lock_guard<std::mutex> lock(mtx_); // Ensure thread safety
        for (size_t i = 0; i < numReplicas_; ++i) {
            // Calculate a unique hash value for each virtual node
            size_t hash = hashFunction_(node +"_0"+std::to_string(i));
            circle_[hash] = node;         // Hash value maps to node
            sortedHashes_.push_back(hash); // Add to sorted list
        }
        // Sort the hash values
        std::sort(sortedHashes_.begin(), sortedHashes_.end());
    }

    /**
     * @brief Remove a node from the hash ring.
     *
     * Delete all virtual nodes of the node and their corresponding hash values.
     *
     * @param node Name of the node to remove.
     */
    void removeNode(const std::string& node) {
        std::lock_guard<std::mutex> lock(mtx_); // Ensure thread safety
        for (size_t i = 0; i < numReplicas_; ++i) {
            // Calculate the hash value of the virtual node
            size_t hash = hashFunction_(node + std::to_string(i));
            circle_.erase(hash); // Remove the hash from the hash ring
            auto it = std::find(sortedHashes_.begin(), sortedHashes_.end(), hash);
            if (it != sortedHashes_.end()) {
                sortedHashes_.erase(it); // Remove from sorted list
            }
        }
    }

    /**
     * @brief Find the node responsible for handling the given key.
     *
     * Find the first node in the hash ring whose hash value is greater than or equal to the key's hash value.
     * If not found (i.e., exceeds the maximum value of the hash ring), wrap around to the first node.
     *
     * @param key The key to look up (e.g., data identifier).
     * @return The name of the node responsible for the key.
     * @throws std::runtime_error If the hash ring is empty (no nodes).
     */
    size_t getNode(const std::string& key) {
        std::lock_guard<std::mutex> lock(mtx_); // Ensure thread safety
    if (circle_.empty()) {
            throw std::runtime_error("No nodes in consistent hash"); // Throw exception if ring is empty
    }
        size_t hash = hashFunction_(key); // Calculate the hash value of the key
        // Find the first position in the sorted hash list greater than the key's hash value
    auto it = std::upper_bound(sortedHashes_.begin(), sortedHashes_.end(), hash);
    if (it == sortedHashes_.end()) {
            // If it exceeds the maximum value of the ring, wrap around to the first node
        it = sortedHashes_.begin();
    }
        return *it; // Return the corresponding hash value
    }

private:
    size_t numReplicas_; // Number of virtual nodes per physical node
    std::function<size_t(const std::string&)> hashFunction_; // User-defined or default hash function
    std::unordered_map<size_t, std::string> circle_; // Mapping from hash value to node name
    std::vector<size_t> sortedHashes_; // Sorted list of hash values for efficient lookup
    std::mutex mtx_; // Mutex to protect the hash ring and ensure thread safety
};
