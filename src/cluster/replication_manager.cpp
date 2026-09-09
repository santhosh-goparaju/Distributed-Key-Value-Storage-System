#include "cluster/replication_manager.h"
#include <algorithm>

namespace kvstore {

ReplicationManager::ReplicationManager(ClusterManager& cluster, ConcurrentKVStore& store,
                                       WriteAheadLog& wal, size_t replication_factor)
    : cluster_(cluster), store_(store), wal_(wal), replication_factor_(replication_factor) {
    write_quorum_ = replication_factor_ / 2 + 1;
}

Response ReplicationManager::handlePut(const Key& key, const Value& value) {
    std::vector<NodeInfo> replicas = cluster_.getReplicaNodes(key, replication_factor_);
    size_t acks = 0;
    
    bool local_write = false;
    for (const auto& node : replicas) {
        if (node.id == cluster_.selfNode().id) {
            local_write = true;
            break;
        }
    }
    
    if (local_write) {
        // Assume WAL append is successful for now
        // wal_.append(Command::PUT, key, value);
        Response resp = store_.put(key, value);
        if (resp.status == Status::OK) {
            acks++;
        }
    }
    
    Request req{Command::PUT, key, value};
    acks += replicateToNodes(replicas, req);
    
    if (acks >= write_quorum_) {
        return {Status::OK, ""};
    } else {
        return {Status::ERROR, ""};
    }
}

Response ReplicationManager::handleGet(const Key& key) {
    if (cluster_.isLocalKey(key)) {
        return store_.get(key);
    }
    
    std::vector<NodeInfo> replicas = cluster_.getReplicaNodes(key, replication_factor_);
    for (const auto& node : replicas) {
        if (node.id == cluster_.selfNode().id) {
            Response resp = store_.get(key);
            if (resp.status == Status::OK) return resp;
        } else {
            Request req{Command::GET, key, ""};
            Response resp = cluster_.forwardRequest(node, req);
            if (resp.status == Status::OK) {
                return resp;
            }
        }
    }
    return {Status::NOT_FOUND, ""};
}

Response ReplicationManager::handleDelete(const Key& key) {
    std::vector<NodeInfo> replicas = cluster_.getReplicaNodes(key, replication_factor_);
    size_t acks = 0;
    
    bool local_write = false;
    for (const auto& node : replicas) {
        if (node.id == cluster_.selfNode().id) {
            local_write = true;
            break;
        }
    }
    
    if (local_write) {
        Response resp = store_.del(key);
        if (resp.status == Status::OK || resp.status == Status::NOT_FOUND) {
            acks++;
        }
    }
    
    Request req{Command::DELETE, key, ""};
    acks += replicateToNodes(replicas, req);
    
    if (acks >= write_quorum_) {
        return {Status::OK, ""};
    } else {
        return {Status::ERROR, ""};
    }
}

void ReplicationManager::setWriteQuorum(size_t quorum) {
    write_quorum_ = quorum;
}

size_t ReplicationManager::replicationFactor() const {
    return replication_factor_;
}

size_t ReplicationManager::replicateToNodes(const std::vector<NodeInfo>& nodes, const Request& req) {
    size_t acks = 0;
    for (const auto& node : nodes) {
        if (node.id != cluster_.selfNode().id) {
            Response resp = cluster_.forwardRequest(node, req);
            if (resp.status == Status::OK || resp.status == Status::NOT_FOUND) {
                acks++;
            }
        }
    }
    return acks;
}

} // namespace kvstore
