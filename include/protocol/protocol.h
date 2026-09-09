#pragma once
#include "common/types.h"
#include <vector>
#include <cstdint>
#include <optional>

namespace kvstore {
namespace protocol {

// Wire format:
// Request:  [4-byte total_len][1-byte command][4-byte key_len][key_bytes][4-byte val_len][val_bytes]
// Response: [4-byte total_len][1-byte status][4-byte val_len][val_bytes]
// All multi-byte integers in network byte order (big-endian)

std::vector<uint8_t> serializeRequest(const Request& req);
std::optional<Request> deserializeRequest(const uint8_t* data, size_t len);

std::vector<uint8_t> serializeResponse(const Response& resp);
std::optional<Response> deserializeResponse(const uint8_t* data, size_t len);

// Read a length-prefixed frame from a file descriptor
// Returns true on success, false on EOF/error
bool readFrame(int fd, std::vector<uint8_t>& buffer);

// Write a length-prefixed frame to a file descriptor
bool writeFrame(int fd, const std::vector<uint8_t>& data);

} // namespace protocol
} // namespace kvstore
