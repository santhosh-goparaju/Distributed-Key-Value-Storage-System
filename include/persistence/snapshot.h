#pragma once
#include "common/types.h"
#include <string>
#include <unordered_map>

namespace kvstore {

class SnapshotManager {
public:
    explicit SnapshotManager(const std::string& directory);

    // Save full state to snapshot file
    void save(const std::unordered_map<Key, Value>& data);

    // Load state from snapshot file
    std::unordered_map<Key, Value> load() const;

    // Check if a snapshot exists
    bool exists() const;

    // Get snapshot file path
    std::string path() const;

private:
    std::string directory_;
    std::string snapshotPath() const;
};

} // namespace kvstore
