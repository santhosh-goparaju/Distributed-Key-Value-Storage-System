#pragma once
#include "common/types.h"
#include <unordered_map>
#include <mutex>

namespace kvstore {

class KVStore {
public:
    KVStore() = default;
    ~KVStore() = default;

    // Core operations
    Response put(const Key& key, const Value& value);
    Response get(const Key& key) const;
    Response del(const Key& key);

    // Utility
    size_t size() const;
    void clear();
    const std::unordered_map<Key, Value>& data() const;
    void loadData(const std::unordered_map<Key, Value>& d);

private:
    std::unordered_map<Key, Value> store_;
};

} // namespace kvstore
