#include "cluster/consistent_hash.h"
#include <algorithm>

namespace kvstore {

ConsistentHashRing::ConsistentHashRing(size_t virtual_nodes_per_node)
    : virtual_nodes_per_node_(virtual_nodes_per_node) {}

size_t ConsistentHashRing::hash(const std::string& key) const {
    size_t hash_val = 14695981039346656037ULL; // FNV offset basis
    for (char c : key) {
        hash_val ^= static_cast<size_t>(c);
        hash_val *= 1099511628211ULL; // FNV prime
    }
    return hash_val;
}

void ConsistentHashRing::addNode(const NodeInfo& node) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (std::find(nodes_.begin(), nodes_.end(), node.id) == nodes_.end()) {
        nodes_.push_back(node.id);
    }
    for (size_t i = 0; i < virtual_nodes_per_node_; ++i) {
        std::string vnode_key = node.id + "#" + std::to_string(i);
        ring_[hash(vnode_key)] = node.id;
    }
}

void ConsistentHashRing::removeNode(const std::string& node_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    nodes_.erase(std::remove(nodes_.begin(), nodes_.end(), node_id), nodes_.end());
    for (auto it = ring_.begin(); it != ring_.end(); ) {
        if (it->second == node_id) {
            it = ring_.erase(it);
        } else {
            ++it;
        }
    }
}

std::string ConsistentHashRing::getNode(const Key& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (ring_.empty()) return "";
    size_t hash_val = hash(key);
    auto it = ring_.lower_bound(hash_val);
    if (it == ring_.end()) {
        it = ring_.begin();
    }
    return it->second;
}

std::vector<std::string> ConsistentHashRing::getNodes(const Key& key, size_t count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    if (ring_.empty() || count == 0) return result;
    
    size_t hash_val = hash(key);
    auto it = ring_.lower_bound(hash_val);
    if (it == ring_.end()) {
        it = ring_.begin();
    }
    
    auto start_it = it;
    while (result.size() < count && result.size() < nodes_.size()) {
        if (std::find(result.begin(), result.end(), it->second) == result.end()) {
            result.push_back(it->second);
        }
        ++it;
        if (it == ring_.end()) {
            it = ring_.begin();
        }
        if (it == start_it) break;
    }
    return result;
}

size_t ConsistentHashRing::nodeCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return nodes_.size();
}

bool ConsistentHashRing::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ring_.empty();
}

} // namespace kvstore
