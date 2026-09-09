#pragma once
#include "storage/kv_store.h"
#include <shared_mutex>

namespace kvstore {

class ConcurrentKVStore {
public:
    ConcurrentKVStore() = default;
    ~ConcurrentKVStore() = default;

    Response put(const Key& key, const Value& value);
    Response get(const Key& key) const;
    Response del(const Key& key);

    size_t size() const;
    std::unordered_map<Key, Value> snapshot() const;  // atomic copy
    void loadFromSnapshot(const std::unordered_map<Key, Value>& data);

private:
    KVStore store_;
    mutable std::shared_mutex mutex_;
};

} // namespace kvstore
