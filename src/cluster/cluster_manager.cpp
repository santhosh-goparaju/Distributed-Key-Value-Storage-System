#include "cluster/cluster_manager.h"
#include "networking/tcp_connection.h"
#include "protocol/protocol.h"
#include <fstream>
#include <sstream>

namespace kvstore {

ClusterManager::ClusterManager(const std::string& self_id, const std::string& config_path)
    : self_id_(self_id) {
    loadConfig(config_path);
}

void ClusterManager::loadConfig(const std::string& config_path) {
    std::ifstream file(config_path);
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        std::string id, host;
        uint16_t port;
        if (iss >> id >> host >> port) {
            NodeInfo node{id, host, port, NodeInfo::Status::UP};
            nodes_.push_back(node);
            node_index_[id] = nodes_.size() - 1;
            if (id == self_id_) {
                self_node_ = node;
            }
        }
    }
}

void ClusterManager::initialize() {
    for (const auto& node : nodes_) {
        ring_.addNode(node);
    }
}

NodeInfo ClusterManager::getNodeForKey(const Key& key) const {
    std::string node_id = ring_.getNode(key);
    auto node_opt = getNode(node_id);
    if (node_opt) return *node_opt;
    return {};
}

std::vector<NodeInfo> ClusterManager::getReplicaNodes(const Key& key, size_t replica_count) const {
    std::vector<NodeInfo> result;
    std::vector<std::string> node_ids = ring_.getNodes(key, replica_count);
    for (const auto& id : node_ids) {
        auto node_opt = getNode(id);
        if (node_opt && node_opt->status != NodeInfo::Status::DOWN) {
            result.push_back(*node_opt);
        }
    }
    return result;
}

bool ClusterManager::isLocalKey(const Key& key) const {
    return ring_.getNode(key) == self_id_;
}

Response ClusterManager::forwardRequest(const NodeInfo& node, const Request& req) {
    auto conn_opt = TcpConnection::connect(node.host, node.port);
    if (!conn_opt) {
        return {Status::NODE_UNAVAILABLE, ""};
    }
    TcpConnection& conn = *conn_opt;
    
    std::vector<uint8_t> req_data = protocol::serializeRequest(req);
    if (!protocol::writeFrame(conn.fd(), req_data)) {
        return {Status::NODE_UNAVAILABLE, ""};
    }
    
    std::vector<uint8_t> resp_data;
    if (!protocol::readFrame(conn.fd(), resp_data)) {
        return {Status::NODE_UNAVAILABLE, ""};
    }
    
    auto resp_opt = protocol::deserializeResponse(resp_data.data(), resp_data.size());
    if (resp_opt) {
        return *resp_opt;
    }
    return {Status::NODE_UNAVAILABLE, ""};
}

const std::vector<NodeInfo>& ClusterManager::nodes() const {
    return nodes_;
}

const NodeInfo& ClusterManager::selfNode() const {
    return self_node_;
}

void ClusterManager::setNodeStatus(const std::string& node_id, NodeInfo::Status status) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = node_index_.find(node_id);
    if (it != node_index_.end()) {
        nodes_[it->second].status = status;
    }
}

std::optional<NodeInfo> ClusterManager::getNode(const std::string& node_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = node_index_.find(node_id);
    if (it != node_index_.end()) {
        return nodes_[it->second];
    }
    return std::nullopt;
}

const ConsistentHashRing& ClusterManager::ring() const {
    return ring_;
}

} // namespace kvstore
