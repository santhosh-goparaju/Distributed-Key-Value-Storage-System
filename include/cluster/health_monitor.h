#pragma once
#include "cluster/cluster_manager.h"
#include <thread>
#include <atomic>
#include <chrono>

namespace kvstore {

class HealthMonitor {
public:
    HealthMonitor(ClusterManager& cluster,
                  std::chrono::milliseconds interval = std::chrono::milliseconds(2000));
    ~HealthMonitor();

    // Start monitoring in background thread
    void start();

    // Stop monitoring
    void stop();

    // Check if a specific node is healthy
    bool isNodeHealthy(const std::string& node_id) const;

private:
    void monitorLoop();
    bool pingNode(const NodeInfo& node);

    ClusterManager& cluster_;
    std::chrono::milliseconds interval_;
    std::thread monitor_thread_;
    std::atomic<bool> running_{false};
};

} // namespace kvstore
