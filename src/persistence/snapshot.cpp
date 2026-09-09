#include "persistence/snapshot.h"
#include <filesystem>
#include <fstream>

namespace kvstore {

SnapshotManager::SnapshotManager(const std::string& directory) : directory_(directory) {}

std::string SnapshotManager::snapshotPath() const {
    return directory_ + "/snapshot.dat";
}

std::string SnapshotManager::path() const {
    return snapshotPath();
}

bool SnapshotManager::exists() const {
    return std::filesystem::exists(snapshotPath());
}

void SnapshotManager::save(const std::unordered_map<Key, Value>& data) {
    if (!std::filesystem::exists(directory_)) {
        std::filesystem::create_directories(directory_);
    }
    std::ofstream out(snapshotPath(), std::ios::binary | std::ios::trunc);
    uint32_t num_entries = data.size();
    out.write(reinterpret_cast<const char*>(&num_entries), sizeof(num_entries));
    
    for (const auto& [k, v] : data) {
        uint32_t key_len = k.size();
        out.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
        out.write(k.data(), key_len);
        
        uint32_t val_len = v.size();
        out.write(reinterpret_cast<const char*>(&val_len), sizeof(val_len));
        out.write(v.data(), val_len);
    }
}

std::unordered_map<Key, Value> SnapshotManager::load() const {
    std::unordered_map<Key, Value> result;
    std::ifstream in(snapshotPath(), std::ios::binary);
    if (!in) return result;
    
    uint32_t num_entries = 0;
    if (!in.read(reinterpret_cast<char*>(&num_entries), sizeof(num_entries))) {
        return result;
    }
    
    for (uint32_t i = 0; i < num_entries; ++i) {
        uint32_t key_len = 0;
        if (!in.read(reinterpret_cast<char*>(&key_len), sizeof(key_len))) break;
        std::string key(key_len, '\0');
        in.read(&key[0], key_len);
        
        uint32_t val_len = 0;
        if (!in.read(reinterpret_cast<char*>(&val_len), sizeof(val_len))) break;
        std::string val(val_len, '\0');
        in.read(&val[0], val_len);
        
        result[key] = val;
    }
    return result;
}

} // namespace kvstore
