#pragma once
#include "cluster/node.h"
#include "common/types.h"
#include <map>
#include <vector>
#include <string>
#include <mutex>
#include <functional>

namespace kvstore {

class ConsistentHashRing {
public:
    explicit ConsistentHashRing(size_t virtual_nodes_per_node = 150);

    // Add a node to the ring
    void addNode(const NodeInfo& node);

    // Remove a node from the ring
    void removeNode(const std::string& node_id);

    // Get the primary node for a key
    std::string getNode(const Key& key) const;

    // Get N nodes for a key (for replication: primary + N-1 replicas)
    // Returns unique physical nodes, walking clockwise around the ring
    std::vector<std::string> getNodes(const Key& key, size_t count) const;

    // Number of physical nodes
    size_t nodeCount() const;

    // Check if ring is empty
    bool empty() const;

private:
    size_t hash(const std::string& key) const;

    size_t virtual_nodes_per_node_;
    std::map<size_t, std::string> ring_;  // hash -> node_id
    std::vector<std::string> nodes_;
    mutable std::mutex mutex_;
};

} // namespace kvstore
