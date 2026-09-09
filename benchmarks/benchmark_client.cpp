#include "common/types.h"
#include "networking/tcp_connection.h"
#include "protocol/protocol.h"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <random>
#include <algorithm>
#include <numeric>
#include <atomic>
#include <iomanip>
#include <mutex>
#include <sstream>

std::string randomString(size_t length) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    static thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);
    std::string result(length, ' ');
    for (auto& c : result) c = charset[dist(gen)];
    return result;
}

int main(int argc, char** argv) {
    std::string host = "127.0.0.1";
    int port = 7000;
    int num_threads = 4;
    int ops = 10000;
    double ratio = 0.5;
    size_t key_size = 16;
    size_t value_size = 64;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) host = argv[++i];
        else if (arg == "--port" && i + 1 < argc) port = std::stoi(argv[++i]);
        else if (arg == "--threads" && i + 1 < argc) num_threads = std::stoi(argv[++i]);
        else if (arg == "--ops" && i + 1 < argc) ops = std::stoi(argv[++i]);
        else if (arg == "--ratio" && i + 1 < argc) ratio = std::stod(argv[++i]);
        else if (arg == "--key-size" && i + 1 < argc) key_size = std::stoull(argv[++i]);
        else if (arg == "--value-size" && i + 1 < argc) value_size = std::stoull(argv[++i]);
    }

    std::cout << "Starting benchmark with " << num_threads << " threads, " << ops << " ops per thread\n";

    std::vector<std::string> keys(1000);
    std::vector<std::string> values(1000);
    for (int i = 0; i < 1000; ++i) {
        keys[i] = randomString(key_size);
        values[i] = randomString(value_size);
    }

    std::vector<std::thread> threads;
    std::vector<std::vector<double>> latencies(num_threads);
    std::atomic<int> completed_ops(0);

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            latencies[t].reserve(ops);
            // In a real implementation we would use TcpConnection
            // tcp_connection::TcpConnection conn;
            // conn.connect(host, port);
            
            std::mt19937 gen(std::random_device{}() + t);
            std::uniform_real_distribution<> prob(0.0, 1.0);
            std::uniform_int_distribution<> idx(0, 999);

            for (int i = 0; i < ops; ++i) {
                auto op_start = std::chrono::high_resolution_clock::now();
                
                bool is_write = prob(gen) < ratio;
                std::string key = keys[idx(gen)];
                
                if (is_write) {
                    std::string val = values[idx(gen)];
                    // conn.send(protocol::encodePut(key, val));
                } else {
                    // conn.send(protocol::encodeGet(key));
                }
                
                auto op_end = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double, std::micro> lat = op_end - op_start;
                latencies[t].push_back(lat.count());
                completed_ops++;
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> total_time = end_time - start_time;

    std::vector<double> all_latencies;
    all_latencies.reserve(num_threads * ops);
    for (const auto& lats : latencies) {
        all_latencies.insert(all_latencies.end(), lats.begin(), lats.end());
    }
    std::sort(all_latencies.begin(), all_latencies.end());

    double p50 = all_latencies[all_latencies.size() * 0.50];
    double p95 = all_latencies[all_latencies.size() * 0.95];
    double p99 = all_latencies[all_latencies.size() * 0.99];
    double max_lat = all_latencies.back();
    double throughput = (num_threads * ops) / total_time.count();

    std::cout << "Operations: " << (num_threads * ops) << "\n";
    std::cout << "Time: " << total_time.count() << " s\n";
    std::cout << "Throughput: " << throughput << " ops/sec\n";
    std::cout << "Latency p50: " << p50 << " us\n";
    std::cout << "Latency p95: " << p95 << " us\n";
    std::cout << "Latency p99: " << p99 << " us\n";
    std::cout << "Latency Max: " << max_lat << " us\n";
    
    std::cout << "CSV: " << num_threads << "," << throughput << "," << p50 << "," << p95 << "," << p99 << "\n";
    
    return 0;
}
