#include "networking/tcp_server.h"
#include "networking/tcp_connection.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <cstring>
#include <iostream>

namespace kvstore {

TcpServer::TcpServer(const std::string& host, uint16_t port, size_t thread_pool_size)
    : host_(host), port_(port), thread_pool_(thread_pool_size) {}

TcpServer::~TcpServer() {
    stop();
}

void TcpServer::setHandler(RequestHandler handler) {
    handler_ = std::move(handler);
}

void TcpServer::start() {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "Failed to create socket\n";
        return;
    }

    int opt = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "setsockopt SO_REUSEADDR failed\n";
    }
#ifdef SO_REUSEPORT
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        std::cerr << "setsockopt SO_REUSEPORT failed\n";
    }
#endif

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(host_.c_str());
    if (addr.sin_addr.s_addr == INADDR_NONE) {
        addr.sin_addr.s_addr = INADDR_ANY;
    }
    addr.sin_port = htons(port_);

    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Bind failed\n";
        ::close(server_fd_);
        server_fd_ = -1;
        return;
    }

    if (listen(server_fd_, 128) < 0) {
        std::cerr << "Listen failed\n";
        ::close(server_fd_);
        server_fd_ = -1;
        return;
    }

    socklen_t len = sizeof(addr);
    if (getsockname(server_fd_, (struct sockaddr*)&addr, &len) == 0) {
        port_ = ntohs(addr.sin_port);
    }

    running_ = true;
    acceptLoop();
}

void TcpServer::acceptLoop() {
    struct pollfd pfd;
    pfd.fd = server_fd_;
    pfd.events = POLLIN;

    while (running_) {
        int ret = poll(&pfd, 1, 100);
        if (ret > 0 && (pfd.revents & POLLIN)) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd >= 0) {
                thread_pool_.submit([this, client_fd]() {
                    handleConnection(client_fd);
                });
            }
        }
    }
}

void TcpServer::handleConnection(int client_fd) {
    TcpConnection conn(client_fd);
    while (conn.isOpen()) {
        auto req = conn.readRequest();
        if (!req) {
            break;
        }
        Response resp = {Status::ERROR, ""};
        if (handler_) {
            resp = handler_(*req);
        }
        if (!conn.sendResponse(resp)) {
            break;
        }
    }
}

void TcpServer::stop() {
    running_ = false;
    if (server_fd_ >= 0) {
        ::close(server_fd_);
        server_fd_ = -1;
    }
}

uint16_t TcpServer::port() const {
    return port_;
}

} // namespace kvstore
