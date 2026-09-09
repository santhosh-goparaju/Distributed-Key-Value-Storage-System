#pragma once
#include <string>
#include <cstdint>

namespace kvstore {

struct NodeInfo {
    std::string id;
    std::string host;
    uint16_t port = 0;

    enum class Status {
        UP,
        SUSPECT,
        DOWN
    };
    Status status = Status::UP;

    std::string address() const {
        return host + ":" + std::to_string(port);
    }

    bool operator==(const NodeInfo& other) const {
        return id == other.id;
    }
};

} // namespace kvstore
