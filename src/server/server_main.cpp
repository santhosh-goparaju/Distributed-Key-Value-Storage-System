#include "common/types.h"
#include "storage/concurrent_kv_store.h"
#include "persistence/wal.h"
#include "persistence/snapshot.h"
#include "networking/tcp_server.h"
#include "cluster/cluster_manager.h"
#include "cluster/replication_manager.h"
#include "cluster/health_monitor.h"
#include <iostream>
#include <filesystem>
#include <csignal>
#include <memory>
#include <string>

using namespace kvstore;

std::unique_ptr<TcpServer> global_server;

void signalHandler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received. Shutting down...\n";
    if (global_server) {
        global_server->stop();
    }
}

int main(int argc, char* argv[]) {
    std::string host = "0.0.0.0";
    uint16_t port = 7000;
    size_t threads = 8;
    std::string data_dir = "./data";
    std::string cluster_config;
    std::string node_id;
    size_t replication_factor = 3;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) host = argv[++i];
        else if (arg == "--port" && i + 1 < argc) port = static_cast<uint16_t>(std::stoi(argv[++i]));
        else if (arg == "--threads" && i + 1 < argc) threads = static_cast<size_t>(std::stoi(argv[++i]));
        else if (arg == "--data-dir" && i + 1 < argc) data_dir = argv[++i];
        else if (arg == "--cluster-config" && i + 1 < argc) cluster_config = argv[++i];
        else if (arg == "--node-id" && i + 1 < argc) node_id = argv[++i];
        else if (arg == "--replication" && i + 1 < argc) replication_factor = static_cast<size_t>(std::stoi(argv[++i]));
        else {
            std::cerr << "Usage: kv-server [options]\n"
                      << "  --host <host>         Bind address (default: 0.0.0.0)\n"
                      << "  --port <port>         Bind port (default: 7000)\n"
                      << "  --threads <n>         Thread pool size (default: 8)\n"
                      << "  --data-dir <path>     Data directory for WAL/snapshots (default: ./data)\n"
                      << "  --cluster-config <f>  Cluster config file (optional, enables cluster mode)\n"
                      << "  --node-id <id>        This node's ID (required in cluster mode)\n"
                      << "  --replication <n>     Replication factor (default: 3, cluster mode only)\n";
            return 1;
        }
    }

    // Create data directory
    std::filesystem::create_directories(data_dir);

    // Initialize storage and persistence
    ConcurrentKVStore store;
    SnapshotManager snapshot_mgr(data_dir);
    WriteAheadLog wal(data_dir + "/wal.log");

    // Recovery: load snapshot, then replay WAL
    if (snapshot_mgr.exists()) {
        std::cout << "Loading snapshot...\n";
        store.loadFromSnapshot(snapshot_mgr.load());
        std::cout << "Snapshot loaded: " << store.size() << " keys\n";
    }
    size_t replayed = wal.replay(store);
    if (replayed > 0) {
        std::cout << "Replayed " << replayed << " WAL entries\n";
    }

    // Create TCP server
    global_server = std::make_unique<TcpServer>(host, port, threads);

    // Cluster components (kept alive for the lifetime of the server)
    std::unique_ptr<ClusterManager> cluster_manager;
    std::unique_ptr<ReplicationManager> replication_mgr;
    std::unique_ptr<HealthMonitor> health_monitor;

    if (!cluster_config.empty()) {
        if (node_id.empty()) {
            std::cerr << "Error: --node-id is required in cluster mode\n";
            return 1;
        }

        cluster_manager = std::make_unique<ClusterManager>(node_id, cluster_config);
        cluster_manager->initialize();

        replication_mgr = std::make_unique<ReplicationManager>(
            *cluster_manager, store, wal, replication_factor);

        health_monitor = std::make_unique<HealthMonitor>(*cluster_manager);
        health_monitor->start();

        // Capture raw pointers for the lambda (objects outlive the lambda)
        auto* rep_mgr = replication_mgr.get();
        global_server->setHandler([rep_mgr](const Request& req) -> Response {
            switch (req.command) {
                case Command::PUT: return rep_mgr->handlePut(req.key, req.value);
                case Command::GET: return rep_mgr->handleGet(req.key);
                case Command::DELETE: return rep_mgr->handleDelete(req.key);
                case Command::PING: return {Status::OK, "PONG"};
                default: return {Status::INVALID_REQUEST, ""};
            }
        });
    } else {
        // Standalone mode handler
        global_server->setHandler([&store, &wal](const Request& req) -> Response {
            switch (req.command) {
                case Command::PUT: {
                    auto resp = store.put(req.key, req.value);
                    if (resp.status == Status::OK) wal.append(Command::PUT, req.key, req.value);
                    return resp;
                }
                case Command::GET:
                    return store.get(req.key);
                case Command::DELETE: {
                    auto resp = store.del(req.key);
                    if (resp.status == Status::OK) wal.append(Command::DELETE, req.key);
                    return resp;
                }
                case Command::PING:
                    return {Status::OK, "PONG"};
                default:
                    return {Status::INVALID_REQUEST, ""};
            }
        });
    }

    // Set up signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Print startup banner
    std::cout << "========================================\n";
    std::cout << "  Distributed Key-Value Store v1.0\n";
    std::cout << "========================================\n";
    std::cout << "Host:       " << host << "\n";
    std::cout << "Port:       " << port << "\n";
    std::cout << "Threads:    " << threads << "\n";
    std::cout << "Data Dir:   " << data_dir << "\n";
    std::cout << "Mode:       " << (cluster_config.empty() ? "Standalone" : "Cluster") << "\n";
    if (!cluster_config.empty()) {
        std::cout << "Node ID:    " << node_id << "\n";
        std::cout << "Replication: " << replication_factor << "\n";
    }
    std::cout << "Keys:       " << store.size() << "\n";
    std::cout << "========================================\n";
    std::cout << "Server started. Press Ctrl+C to stop.\n";

    // Start server (blocking)
    global_server->start();

    // Graceful shutdown: save snapshot
    std::cout << "Saving snapshot...\n";
    snapshot_mgr.save(store.snapshot());
    wal.truncate();
    std::cout << "Server stopped gracefully.\n";

    if (health_monitor) health_monitor->stop();

    return 0;
}
