#include "networking/tcp_connection.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>

namespace kvstore {

TcpConnection::TcpConnection(int fd) : fd_(fd), open_(true) {}

TcpConnection::~TcpConnection() {
    close();
}

TcpConnection::TcpConnection(TcpConnection&& other) noexcept : fd_(other.fd_), open_(other.open_) {
    other.fd_ = -1;
    other.open_ = false;
}

TcpConnection& TcpConnection::operator=(TcpConnection&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        open_ = other.open_;
        other.fd_ = -1;
        other.open_ = false;
    }
    return *this;
}

std::optional<Request> TcpConnection::readRequest() {
    if (!open_) return std::nullopt;
    std::vector<uint8_t> buffer;
    if (!protocol::readFrame(fd_, buffer)) {
        close();
        return std::nullopt;
    }
    return protocol::deserializeRequest(buffer.data(), buffer.size());
}

bool TcpConnection::sendResponse(const Response& response) {
    if (!open_) return false;
    std::vector<uint8_t> data = protocol::serializeResponse(response);
    if (!protocol::writeFrame(fd_, data)) {
        close();
        return false;
    }
    return true;
}

void TcpConnection::close() {
    if (open_) {
        ::close(fd_);
        open_ = false;
    }
}

bool TcpConnection::isOpen() const {
    return open_;
}

int TcpConnection::fd() const {
    return fd_;
}

std::optional<TcpConnection> TcpConnection::connect(const std::string& host, uint16_t port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return std::nullopt;
    
    struct hostent* server = gethostbyname(host.c_str());
    if (!server) {
        ::close(sock);
        return std::nullopt;
    }
    
    struct sockaddr_in serv_addr;
    std::memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    std::memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_port = htons(port);
    
    if (::connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        ::close(sock);
        return std::nullopt;
    }
    
    return TcpConnection(sock);
}

} // namespace kvstore
