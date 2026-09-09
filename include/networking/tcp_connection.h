#pragma once
#include "common/types.h"
#include "protocol/protocol.h"
#include <string>
#include <optional>

namespace kvstore {

class TcpConnection {
public:
    explicit TcpConnection(int fd);
    ~TcpConnection();

    // Disable copy, enable move
    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;
    TcpConnection(TcpConnection&& other) noexcept;
    TcpConnection& operator=(TcpConnection&& other) noexcept;

    // Read a request from the connection
    std::optional<Request> readRequest();

    // Send a response
    bool sendResponse(const Response& response);

    // Close the connection
    void close();

    // Check if connection is still open
    bool isOpen() const;

    // Get the file descriptor
    int fd() const;

    // Client-side: connect to a server
    static std::optional<TcpConnection> connect(const std::string& host, uint16_t port);

private:
    int fd_;
    bool open_;
};

} // namespace kvstore
