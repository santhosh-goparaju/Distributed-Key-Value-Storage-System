#pragma once
#include "common/types.h"
#include "networking/thread_pool.h"
#include <string>
#include <functional>
#include <atomic>
#include <cstdint>

namespace kvstore {

class TcpServer {
public:
    using RequestHandler = std::function<Response(const Request&)>;

    TcpServer(const std::string& host, uint16_t port, size_t thread_pool_size = 8);
    ~TcpServer();

    // Set the request handler
    void setHandler(RequestHandler handler);

    // Start the server (blocking)
    void start();

    // Stop the server
    void stop();

    // Get the actual bound port (useful when port 0 is used)
    uint16_t port() const;

private:
    void acceptLoop();
    void handleConnection(int client_fd);

    std::string host_;
    uint16_t port_;
    int server_fd_ = -1;
    std::atomic<bool> running_{false};
    ThreadPool thread_pool_;
    RequestHandler handler_;
};

} // namespace kvstore
