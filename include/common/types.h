#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <optional>
#include <functional>

namespace kvstore {

using Key = std::string;
using Value = std::string;

constexpr size_t MAX_KEY_SIZE = 256;       // 256 bytes
constexpr size_t MAX_VALUE_SIZE = 65536;   // 64 KB

enum class Status : uint8_t {
    OK = 0,
    NOT_FOUND = 1,
    ERROR = 2,
    KEY_TOO_LARGE = 3,
    VALUE_TOO_LARGE = 4,
    INVALID_REQUEST = 5,
    NODE_UNAVAILABLE = 6,
};

enum class Command : uint8_t {
    PUT = 1,
    GET = 2,
    DELETE = 3,
    PING = 4,
};

struct Request {
    Command command;
    Key key;
    Value value;  // empty for GET/DELETE
};

struct Response {
    Status status;
    Value value;  // empty for PUT/DELETE
};

// String conversion helpers
inline const char* statusToString(Status s) {
    switch (s) {
        case Status::OK: return "OK";
        case Status::NOT_FOUND: return "NOT_FOUND";
        case Status::ERROR: return "ERROR";
        case Status::KEY_TOO_LARGE: return "KEY_TOO_LARGE";
        case Status::VALUE_TOO_LARGE: return "VALUE_TOO_LARGE";
        case Status::INVALID_REQUEST: return "INVALID_REQUEST";
        case Status::NODE_UNAVAILABLE: return "NODE_UNAVAILABLE";
        default: return "UNKNOWN";
    }
}

inline const char* commandToString(Command c) {
    switch (c) {
        case Command::PUT: return "PUT";
        case Command::GET: return "GET";
        case Command::DELETE: return "DELETE";
        case Command::PING: return "PING";
        default: return "UNKNOWN";
    }
}

} // namespace kvstore
