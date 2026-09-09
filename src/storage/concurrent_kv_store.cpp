#include "storage/concurrent_kv_store.h"

namespace kvstore {

Response ConcurrentKVStore::put(const Key& key, const Value& value) {
    std::unique_lock lock(mutex_);
    return store_.put(key, value);
}

Response ConcurrentKVStore::get(const Key& key) const {
    std::shared_lock lock(mutex_);
    return store_.get(key);
}

Response ConcurrentKVStore::del(const Key& key) {
    std::unique_lock lock(mutex_);
    return store_.del(key);
}

size_t ConcurrentKVStore::size() const {
    std::shared_lock lock(mutex_);
    return store_.size();
}

std::unordered_map<Key, Value> ConcurrentKVStore::snapshot() const {
    std::shared_lock lock(mutex_);
    return store_.data();
}

void ConcurrentKVStore::loadFromSnapshot(const std::unordered_map<Key, Value>& data) {
    std::unique_lock lock(mutex_);
    store_.clear();
    store_.loadData(data);
}

} // namespace kvstore
