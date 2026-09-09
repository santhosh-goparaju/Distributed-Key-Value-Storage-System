#include "persistence/wal.h"
#include <filesystem>
#include <unistd.h>
#include <fcntl.h>

namespace kvstore {

WriteAheadLog::WriteAheadLog(const std::string& path) : path_(path) {
    writer_.open(path_, std::ios::binary | std::ios::app);
}

WriteAheadLog::~WriteAheadLog() {
    if (writer_.is_open()) {
        writer_.close();
    }
}

uint32_t WriteAheadLog::crc32(const void* data, size_t length) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= bytes[i];
        for (int j = 0; j < 8; ++j) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return ~crc;
}

void WriteAheadLog::append(Command cmd, const Key& key, const Value& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    uint32_t key_len = key.size();
    uint32_t val_len = value.size();
    uint8_t c = static_cast<uint8_t>(cmd);
    
    uint32_t entry_len = sizeof(c) + sizeof(key_len) + key_len + sizeof(val_len) + val_len;
    
    std::string buffer;
    buffer.reserve(entry_len);
    buffer.append(reinterpret_cast<const char*>(&c), sizeof(c));
    buffer.append(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
    buffer.append(key.data(), key_len);
    buffer.append(reinterpret_cast<const char*>(&val_len), sizeof(val_len));
    buffer.append(value.data(), val_len);
    
    uint32_t crc = crc32(buffer.data(), buffer.size());
    
    writer_.write(reinterpret_cast<const char*>(&entry_len), sizeof(entry_len));
    writer_.write(buffer.data(), buffer.size());
    writer_.write(reinterpret_cast<const char*>(&crc), sizeof(crc));
    
    writer_.flush();
    entry_count_++;
}

size_t WriteAheadLog::replay(ConcurrentKVStore& store) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ifstream reader(path_, std::ios::binary);
    if (!reader) return 0;

    size_t replayed = 0;
    while (reader) {
        uint32_t entry_len = 0;
        if (!reader.read(reinterpret_cast<char*>(&entry_len), sizeof(entry_len))) {
            break;
        }

        std::string buffer(entry_len, '\0');
        if (!reader.read(&buffer[0], entry_len)) {
            break;
        }

        uint32_t read_crc = 0;
        if (!reader.read(reinterpret_cast<char*>(&read_crc), sizeof(read_crc))) {
            break;
        }

        uint32_t actual_crc = crc32(buffer.data(), buffer.size());
        if (actual_crc != read_crc) {
            // Break on corrupted entry (likely a partial write at end)
            break;
        }

        const char* ptr = buffer.data();
        uint8_t cmd_raw = *reinterpret_cast<const uint8_t*>(ptr);
        ptr += sizeof(uint8_t);

        uint32_t key_len = *reinterpret_cast<const uint32_t*>(ptr);
        ptr += sizeof(uint32_t);
        Key key(ptr, key_len);
        ptr += key_len;

        uint32_t val_len = *reinterpret_cast<const uint32_t*>(ptr);
        ptr += sizeof(uint32_t);
        Value value(ptr, val_len);
        ptr += val_len;

        Command cmd = static_cast<Command>(cmd_raw);
        if (cmd == Command::PUT) {
            store.put(key, value);
        } else if (cmd == Command::DELETE) {
            store.del(key);
        }
        replayed++;
    }
    return replayed;
}

void WriteAheadLog::truncate() {
    std::lock_guard<std::mutex> lock(mutex_);
    writer_.close();
    writer_.open(path_, std::ios::binary | std::ios::trunc | std::ios::out);
    entry_count_ = 0;
}

void WriteAheadLog::sync() {
    std::lock_guard<std::mutex> lock(mutex_);
    writer_.flush();
    int fd = ::open(path_.c_str(), O_RDONLY);
    if (fd != -1) {
        ::fsync(fd);
        ::close(fd);
    }
}

size_t WriteAheadLog::entryCount() const {
    return entry_count_;
}

} // namespace kvstore
