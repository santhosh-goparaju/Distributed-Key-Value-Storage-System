#pragma once
#include "cluster/cluster_manager.h"
#include "storage/concurrent_kv_store.h"
#include "persistence/wal.h"
#include "common/types.h"
#include <cstddef>

namespace kvstore {

class ReplicationManager {
public:
    ReplicationManager(ClusterManager& cluster, ConcurrentKVStore& store,
                       WriteAheadLog& wal, size_t replication_factor = 3);

    // Handle a PUT with replication
    Response handlePut(const Key& key, const Value& value);

    // Handle a GET (try local first, then replicas on failure)
    Response handleGet(const Key& key);

    // Handle a DELETE with replication
    Response handleDelete(const Key& key);

    // Set write quorum (number of acks needed for success)
    void setWriteQuorum(size_t quorum);

    // Get replication factor
    size_t replicationFactor() const;

private:
    // Replicate a mutation to replica nodes
    size_t replicateToNodes(const std::vector<NodeInfo>& nodes, const Request& req);

    ClusterManager& cluster_;
    ConcurrentKVStore& store_;
    WriteAheadLog& wal_;
    size_t replication_factor_;
    size_t write_quorum_;  // default: replication_factor / 2 + 1 (majority)
};

} // namespace kvstore
