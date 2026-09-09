#include "cluster/health_monitor.h"
#include "networking/tcp_connection.h"
#include "protocol/protocol.h"
#include <chrono>

namespace kvstore {

HealthMonitor::HealthMonitor(ClusterManager& cluster, std::chrono::milliseconds interval)
    : cluster_(cluster), interval_(interval) {}

HealthMonitor::~HealthMonitor() {
    stop();
}

void HealthMonitor::start() {
    if (!running_) {
        running_ = true;
        monitor_thread_ = std::thread(&HealthMonitor::monitorLoop, this);
    }
}

void HealthMonitor::stop() {
    if (running_) {
        running_ = false;
        if (monitor_thread_.joinable()) {
            monitor_thread_.join();
        }
    }
}

void HealthMonitor::monitorLoop() {
    while (running_) {
        const auto& nodes = cluster_.nodes();
        for (const auto& node : nodes) {
            if (node.id == cluster_.selfNode().id) continue;
            
            bool success = pingNode(node);
            auto current_node = cluster_.getNode(node.id);
            if (!current_node) continue;
            
            if (success) {
                if (current_node->status == NodeInfo::Status::DOWN || current_node->status == NodeInfo::Status::SUSPECT) {
                    cluster_.setNodeStatus(node.id, NodeInfo::Status::UP);
                }
            } else {
                if (current_node->status == NodeInfo::Status::UP) {
                    cluster_.setNodeStatus(node.id, NodeInfo::Status::SUSPECT);
                } else if (current_node->status == NodeInfo::Status::SUSPECT) {
                    cluster_.setNodeStatus(node.id, NodeInfo::Status::DOWN);
                }
            }
        }
        std::this_thread::sleep_for(interval_);
    }
}

bool HealthMonitor::pingNode(const NodeInfo& node) {
    auto conn_opt = TcpConnection::connect(node.host, node.port);
    if (!conn_opt) return false;
    
    TcpConnection& conn = *conn_opt;
    Request req{Command::PING, "", ""};
    std::vector<uint8_t> req_data = protocol::serializeRequest(req);
    
    if (!protocol::writeFrame(conn.fd(), req_data)) {
        return false;
    }
    
    std::vector<uint8_t> resp_data;
    if (!protocol::readFrame(conn.fd(), resp_data)) {
        return false;
    }
    
    auto resp_opt = protocol::deserializeResponse(resp_data.data(), resp_data.size());
    if (resp_opt && resp_opt->status == Status::OK) {
        return true;
    }
    return false;
}

bool HealthMonitor::isNodeHealthy(const std::string& node_id) const {
    auto node = cluster_.getNode(node_id);
    return node && node->status == NodeInfo::Status::UP;
}

} // namespace kvstore
