#include "protocol/protocol.h"
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sys/socket.h>

namespace kvstore {
namespace protocol {

std::vector<uint8_t> serializeRequest(const Request& req) {
    size_t key_len = req.key.size();
    size_t val_len = req.value.size();
    std::vector<uint8_t> buf;
    
    buf.push_back(static_cast<uint8_t>(req.command));
    
    uint32_t n_key_len = htonl(key_len);
    uint8_t* k_ptr = reinterpret_cast<uint8_t*>(&n_key_len);
    buf.insert(buf.end(), k_ptr, k_ptr + 4);
    
    buf.insert(buf.end(), req.key.begin(), req.key.end());
    
    uint32_t n_val_len = htonl(val_len);
    uint8_t* v_ptr = reinterpret_cast<uint8_t*>(&n_val_len);
    buf.insert(buf.end(), v_ptr, v_ptr + 4);
    
    buf.insert(buf.end(), req.value.begin(), req.value.end());
    
    return buf;
}

std::optional<Request> deserializeRequest(const uint8_t* data, size_t len) {
    if (len < 1 + 4 + 4) return std::nullopt;
    
    Request req;
    req.command = static_cast<Command>(data[0]);
    
    size_t offset = 1;
    
    uint32_t n_key_len;
    std::memcpy(&n_key_len, data + offset, 4);
    uint32_t key_len = ntohl(n_key_len);
    offset += 4;
    
    if (len < offset + key_len + 4) return std::nullopt;
    
    req.key = std::string(reinterpret_cast<const char*>(data + offset), key_len);
    offset += key_len;
    
    uint32_t n_val_len;
    std::memcpy(&n_val_len, data + offset, 4);
    uint32_t val_len = ntohl(n_val_len);
    offset += 4;
    
    if (len < offset + val_len) return std::nullopt;
    
    req.value = std::string(reinterpret_cast<const char*>(data + offset), val_len);
    
    return req;
}

std::vector<uint8_t> serializeResponse(const Response& resp) {
    size_t val_len = resp.value.size();
    std::vector<uint8_t> buf;
    
    buf.push_back(static_cast<uint8_t>(resp.status));
    
    uint32_t n_val_len = htonl(val_len);
    uint8_t* v_ptr = reinterpret_cast<uint8_t*>(&n_val_len);
    buf.insert(buf.end(), v_ptr, v_ptr + 4);
    
    buf.insert(buf.end(), resp.value.begin(), resp.value.end());
    
    return buf;
}

std::optional<Response> deserializeResponse(const uint8_t* data, size_t len) {
    if (len < 1 + 4) return std::nullopt;
    
    Response resp;
    resp.status = static_cast<Status>(data[0]);
    
    size_t offset = 1;
    uint32_t n_val_len;
    std::memcpy(&n_val_len, data + offset, 4);
    uint32_t val_len = ntohl(n_val_len);
    offset += 4;
    
    if (len < offset + val_len) return std::nullopt;
    
    resp.value = std::string(reinterpret_cast<const char*>(data + offset), val_len);
    
    return resp;
}

bool readFrame(int fd, std::vector<uint8_t>& buffer) {
    uint32_t n_len;
    size_t read_bytes = 0;
    while (read_bytes < 4) {
        ssize_t n = ::read(fd, reinterpret_cast<char*>(&n_len) + read_bytes, 4 - read_bytes);
        if (n <= 0) return false;
        read_bytes += n;
    }
    
    uint32_t len = ntohl(n_len);
    buffer.resize(len);
    
    read_bytes = 0;
    while (read_bytes < len) {
        ssize_t n = ::read(fd, reinterpret_cast<char*>(buffer.data()) + read_bytes, len - read_bytes);
        if (n <= 0) return false;
        read_bytes += n;
    }
    
    return true;
}

bool writeFrame(int fd, const std::vector<uint8_t>& data) {
    uint32_t n_len = htonl(data.size());
    std::vector<uint8_t> frame(4 + data.size());
    std::memcpy(frame.data(), &n_len, 4);
    std::memcpy(frame.data() + 4, data.data(), data.size());
    
    size_t written = 0;
    while (written < frame.size()) {
        ssize_t n = ::write(fd, reinterpret_cast<const char*>(frame.data()) + written, frame.size() - written);
        if (n <= 0) return false;
        written += n;
    }
    
    return true;
}

} // namespace protocol
} // namespace kvstore
