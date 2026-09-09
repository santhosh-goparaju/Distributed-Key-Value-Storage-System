#pragma once
#include "common/types.h"
#include "storage/concurrent_kv_store.h"
#include <fstream>
#include <mutex>
#include <cstdint>

namespace kvstore {

class WriteAheadLog {
public:
    explicit WriteAheadLog(const std::string& path);
    ~WriteAheadLog();

    // Append a mutation to the log
    void append(Command cmd, const Key& key, const Value& value = "");

    // Replay log entries into a store
    size_t replay(ConcurrentKVStore& store);

    // Truncate the log (after snapshot)
    void truncate();

    // Force sync to disk
    void sync();

    // Number of entries since last truncation
    size_t entryCount() const;

private:
    std::string path_;
    std::ofstream writer_;
    std::mutex mutex_;
    size_t entry_count_ = 0;

    // CRC32 for entry integrity
    static uint32_t crc32(const void* data, size_t length);
};

} // namespace kvstore
