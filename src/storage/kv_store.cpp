#include "storage/kv_store.h"

namespace kvstore {

Response KVStore::put(const Key& key, const Value& value) {
    if (key.size() > MAX_KEY_SIZE) {
        return {Status::KEY_TOO_LARGE, ""};
    }
    if (value.size() > MAX_VALUE_SIZE) {
        return {Status::VALUE_TOO_LARGE, ""};
    }
    store_[key] = value;
    return {Status::OK, ""};
}

Response KVStore::get(const Key& key) const {
    auto it = store_.find(key);
    if (it != store_.end()) {
        return {Status::OK, it->second};
    }
    return {Status::NOT_FOUND, ""};
}

Response KVStore::del(const Key& key) {
    if (store_.erase(key)) {
        return {Status::OK, ""};
    }
    return {Status::NOT_FOUND, ""};
}

size_t KVStore::size() const {
    return store_.size();
}

void KVStore::clear() {
    store_.clear();
}

const std::unordered_map<Key, Value>& KVStore::data() const {
    return store_;
}

void KVStore::loadData(const std::unordered_map<Key, Value>& d) {
    store_ = d;
}

} // namespace kvstore
