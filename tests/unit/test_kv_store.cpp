#include <gtest/gtest.h>
#include "storage/kv_store.h"
#include "storage/concurrent_kv_store.h"
#include <thread>
#include <vector>

namespace kvstore {
namespace test {

TEST(KVStoreTest, BasicPutGet) {
    KVStore store;
    EXPECT_EQ(store.put("key1", "val1").status, Status::OK);
    auto resp = store.get("key1");
    EXPECT_EQ(resp.status, Status::OK);
    EXPECT_EQ(resp.value, "val1");
}

TEST(KVStoreTest, OverwriteExisting) {
    KVStore store;
    store.put("k", "v1");
    store.put("k", "v2");
    EXPECT_EQ(store.get("k").value, "v2");
}

TEST(KVStoreTest, GetMissing) {
    KVStore store;
    EXPECT_EQ(store.get("missing").status, Status::NOT_FOUND);
}

TEST(KVStoreTest, DeleteExisting) {
    KVStore store;
    store.put("k", "v");
    EXPECT_EQ(store.del("k").status, Status::OK);
    EXPECT_EQ(store.get("k").status, Status::NOT_FOUND);
}

TEST(KVStoreTest, DeleteMissing) {
    KVStore store;
    EXPECT_EQ(store.del("k").status, Status::NOT_FOUND);
}

TEST(KVStoreTest, EmptyValue) {
    KVStore store;
    store.put("k", "");
    auto resp = store.get("k");
    EXPECT_EQ(resp.status, Status::OK);
    EXPECT_EQ(resp.value, "");
}

TEST(KVStoreTest, KeyTooLarge) {
    KVStore store;
    std::string huge_key(300, 'a');
    EXPECT_EQ(store.put(huge_key, "val").status, Status::KEY_TOO_LARGE);
}

TEST(KVStoreTest, ValueTooLarge) {
    KVStore store;
    std::string huge_val(70000, 'b');
    EXPECT_EQ(store.put("k", huge_val).status, Status::VALUE_TOO_LARGE);
}

TEST(KVStoreTest, SizeTracking) {
    KVStore store;
    EXPECT_EQ(store.size(), 0);
    store.put("k1", "v1");
    EXPECT_EQ(store.size(), 1);
    store.put("k2", "v2");
    EXPECT_EQ(store.size(), 2);
    store.del("k1");
    EXPECT_EQ(store.size(), 1);
}

TEST(KVStoreTest, Clear) {
    KVStore store;
    store.put("k1", "v1");
    store.clear();
    EXPECT_EQ(store.size(), 0);
    EXPECT_EQ(store.get("k1").status, Status::NOT_FOUND);
}

TEST(ConcurrentKVStoreTest, MultipleThreads) {
    ConcurrentKVStore store;
    auto worker = [&store](int id) {
        for (int i = 0; i < 100; ++i) {
            store.put("k" + std::to_string(id) + "_" + std::to_string(i), "v");
        }
    };
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(worker, i);
    }
    for (auto& t : threads) {
        t.join();
    }
    EXPECT_EQ(store.size(), 400);
}

} // namespace test
} // namespace kvstore
