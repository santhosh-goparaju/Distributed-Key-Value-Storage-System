#pragma once
#include "cluster/node.h"
#include "cluster/consistent_hash.h"
#include "common/types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <optional>

namespace kvstore {

class ClusterManager {
public:
    ClusterManager(const std::string& self_id, const std::string& config_path);

    // Initialize: load config, build hash ring
    void initialize();

    // Get the primary node for a key
    NodeInfo getNodeForKey(const Key& key) const;

    // Get replica nodes for a key (including primary)
    std::vector<NodeInfo> getReplicaNodes(const Key& key, size_t replica_count) const;

    // Check if a key should be handled locally
    bool isLocalKey(const Key& key) const;

    // Forward a request to another node
    Response forwardRequest(const NodeInfo& node, const Request& req);

    // Get all nodes
    const std::vector<NodeInfo>& nodes() const;

    // Get self node info
    const NodeInfo& selfNode() const;

    // Update node status
    void setNodeStatus(const std::string& node_id, NodeInfo::Status status);

    // Get node by ID
    std::optional<NodeInfo> getNode(const std::string& node_id) const;

    // Get the consistent hash ring (for testing)
    const ConsistentHashRing& ring() const;

private:
    void loadConfig(const std::string& config_path);

    std::string self_id_;
    NodeInfo self_node_;
    std::vector<NodeInfo> nodes_;
    std::unordered_map<std::string, size_t> node_index_;  // id -> index in nodes_
    ConsistentHashRing ring_;
    mutable std::mutex mutex_;
};

} // namespace kvstore
