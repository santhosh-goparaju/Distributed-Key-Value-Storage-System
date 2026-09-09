#include <gtest/gtest.h>
#include "cluster/consistent_hash.h"
#include "cluster/cluster_manager.h"
#include "cluster/replication_manager.h"
#include "storage/concurrent_kv_store.h"
#include "persistence/wal.h"
#include "common/types.h"
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <set>

using namespace kvstore;

class ClusterTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = std::filesystem::temp_directory_path() / "kvstore_cluster_test";
        std::filesystem::create_directories(test_dir);
        config_path = test_dir / "nodes.conf";
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir);
    }
    
    std::filesystem::path test_dir;
    std::filesystem::path config_path;
};

// ConsistentHashRing Tests
TEST_F(ClusterTest, ConsistentHashRingAddNode) {
    ConsistentHashRing ring;
    EXPECT_EQ(ring.nodeCount(), 0);
    EXPECT_TRUE(ring.empty());
    
    NodeInfo node{"node1", "127.0.0.1", 7001};
    ring.addNode(node);
    
    EXPECT_EQ(ring.nodeCount(), 1);
    EXPECT_FALSE(ring.empty());
}

TEST_F(ClusterTest, ConsistentHashRingGetNode) {
    ConsistentHashRing ring;
    NodeInfo n1{"node1", "127.0.0.1", 7001};
    NodeInfo n2{"node2", "127.0.0.1", 7002};
    ring.addNode(n1);
    ring.addNode(n2);
    
    std::string node_id = ring.getNode("some_key");
    EXPECT_TRUE(node_id == "node1" || node_id == "node2");
}

TEST_F(ClusterTest, ConsistentHashRingDeterministic) {
    ConsistentHashRing ring;
    ring.addNode({"node1", "127.0.0.1", 7001});
    ring.addNode({"node2", "127.0.0.1", 7002});
    ring.addNode({"node3", "127.0.0.1", 7003});
    
    std::string n1 = ring.getNode("key123");
    std::string n2 = ring.getNode("key123");
    EXPECT_EQ(n1, n2);
}

TEST_F(ClusterTest, ConsistentHashRingGetNodes) {
    ConsistentHashRing ring;
    ring.addNode({"node1", "127.0.0.1", 7001});
    ring.addNode({"node2", "127.0.0.1", 7002});
    ring.addNode({"node3", "127.0.0.1", 7003});
    ring.addNode({"node4", "127.0.0.1", 7004});
    
    auto nodes = ring.getNodes("key123", 3);
    EXPECT_EQ(nodes.size(), 3);
    
    std::set<std::string> unique_nodes(nodes.begin(), nodes.end());
    EXPECT_EQ(unique_nodes.size(), 3);
}

TEST_F(ClusterTest, ConsistentHashRingRemoveNode) {
    ConsistentHashRing ring;
    ring.addNode({"node1", "127.0.0.1", 7001});
    ring.addNode({"node2", "127.0.0.1", 7002});
    ring.addNode({"node3", "127.0.0.1", 7003});
    
    std::string n = ring.getNode("key123");
    
    ring.removeNode("node1");
    EXPECT_EQ(ring.nodeCount(), 2);
}

TEST_F(ClusterTest, ConsistentHashRingDistribution) {
    ConsistentHashRing ring;
    ring.addNode({"node1", "127.0.0.1", 7001});
    ring.addNode({"node2", "127.0.0.1", 7002});
    ring.addNode({"node3", "127.0.0.1", 7003});
    
    std::unordered_map<std::string, int> counts;
    for (int i = 0; i < 1000; i++) {
        counts[ring.getNode("key" + std::to_string(i))]++;
    }
    
    EXPECT_EQ(counts.size(), 3u); // All 3 nodes should receive some keys
    int total = 0;
    for (const auto& [node, count] : counts) {
        EXPECT_GT(count, 0); // Each node must receive at least 1 key
        total += count;
    }
    EXPECT_EQ(total, 1000);
}

// ClusterManager Tests
TEST_F(ClusterTest, ClusterManagerInitialize) {
    std::ofstream ofs(config_path);
    ofs << "node1 127.0.0.1 7001\n";
    ofs << "node2 127.0.0.1 7002\n";
    ofs << "node3 127.0.0.1 7003\n";
    ofs.close();
    
    ClusterManager cluster("node1", config_path.string());
    cluster.initialize();
    
    EXPECT_EQ(cluster.nodes().size(), 3);
    EXPECT_EQ(cluster.ring().nodeCount(), 3);
    EXPECT_EQ(cluster.selfNode().id, "node1");
}

TEST_F(ClusterTest, ClusterManagerKeyResolution) {
    std::ofstream ofs(config_path);
    ofs << "node1 127.0.0.1 7001\n";
    ofs << "node2 127.0.0.1 7002\n";
    ofs.close();
    
    ClusterManager cluster("node1", config_path.string());
    cluster.initialize();
    
    NodeInfo n = cluster.getNodeForKey("test_key");
    EXPECT_TRUE(n.id == "node1" || n.id == "node2");
    
    bool local = cluster.isLocalKey("test_key");
    EXPECT_EQ(local, (n.id == "node1"));
    
    auto replicas = cluster.getReplicaNodes("test_key", 2);
    EXPECT_EQ(replicas.size(), 2);
}

// ReplicationManager Tests
// Note: full replication testing requires multiple real servers.
TEST_F(ClusterTest, ReplicationManagerLocalOps) {
    std::ofstream ofs(config_path);
    ofs << "node1 127.0.0.1 7001\n";
    ofs << "node2 127.0.0.1 7002\n";
    ofs.close();
    
    ClusterManager cluster("node1", config_path.string());
    cluster.initialize();
    
    ConcurrentKVStore store;
    WriteAheadLog wal((test_dir / "test.wal").string());
    
    ReplicationManager rep(cluster, store, wal, 2);
    
    // Find a key that belongs to node1
    std::string test_key = "k1";
    while (!cluster.isLocalKey(test_key)) {
        test_key += "1";
    }
    
    rep.handlePut(test_key, "val");
    
    Response get_resp = rep.handleGet(test_key);
    EXPECT_EQ(get_resp.status, Status::OK);
    EXPECT_EQ(get_resp.value, "val");
}
