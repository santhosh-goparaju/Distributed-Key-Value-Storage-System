#include <gtest/gtest.h>
#include "persistence/wal.h"
#include "persistence/snapshot.h"
#include "storage/concurrent_kv_store.h"
#include <filesystem>
#include <fstream>
#include <ctime>

namespace kvstore {
namespace test {

class RecoveryTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir_ = std::filesystem::temp_directory_path() / ("kvstore_test_" + std::to_string(std::time(nullptr)));
        std::filesystem::create_directories(temp_dir_);
    }

    void TearDown() override {
        std::filesystem::remove_all(temp_dir_);
    }

    std::filesystem::path temp_dir_;
};

TEST_F(RecoveryTest, WALAppendReplay) {
    auto wal_path = temp_dir_ / "wal.dat";
    {
        WriteAheadLog wal(wal_path.string());
        wal.append(Command::PUT, "k1", "v1");
        wal.append(Command::PUT, "k2", "v2");
        wal.append(Command::DELETE, "k1", "");
    }
    
    ConcurrentKVStore store;
    WriteAheadLog wal(wal_path.string());
    size_t replayed = wal.replay(store);
    
    EXPECT_EQ(replayed, 3);
    EXPECT_EQ(store.get("k2").value, "v2");
    EXPECT_EQ(store.get("k1").status, Status::NOT_FOUND);
}

TEST_F(RecoveryTest, WALTruncate) {
    auto wal_path = temp_dir_ / "wal.dat";
    WriteAheadLog wal(wal_path.string());
    wal.append(Command::PUT, "k1", "v1");
    wal.truncate();
    
    ConcurrentKVStore store;
    size_t replayed = wal.replay(store);
    EXPECT_EQ(replayed, 0);
}

TEST_F(RecoveryTest, WALCRCIntegrity) {
    auto wal_path = temp_dir_ / "wal.dat";
    {
        WriteAheadLog wal(wal_path.string());
        wal.append(Command::PUT, "k1", "v1");
        wal.append(Command::PUT, "k2", "v2");
    }
    
    // Corrupt file
    std::fstream f(wal_path.string(), std::ios::in | std::ios::out | std::ios::binary);
    f.seekp(-2, std::ios::end); // Corrupt last crc/val
    f.write("X", 1);
    f.close();
    
    ConcurrentKVStore store;
    WriteAheadLog wal(wal_path.string());
    size_t replayed = wal.replay(store);
    
    // Only first entry should succeed
    EXPECT_EQ(replayed, 1);
    EXPECT_EQ(store.get("k1").value, "v1");
    EXPECT_EQ(store.get("k2").status, Status::NOT_FOUND);
}

TEST_F(RecoveryTest, SnapshotSaveLoad) {
    ConcurrentKVStore store;
    store.put("k1", "v1");
    store.put("k2", "v2");
    
    SnapshotManager sm(temp_dir_.string());
    sm.save(store.snapshot());
    
    EXPECT_TRUE(sm.exists());
    
    ConcurrentKVStore store2;
    store2.loadFromSnapshot(sm.load());
    EXPECT_EQ(store2.get("k1").value, "v1");
    EXPECT_EQ(store2.get("k2").value, "v2");
}

TEST_F(RecoveryTest, FullRecoveryFlow) {
    SnapshotManager sm(temp_dir_.string());
    auto wal_path = temp_dir_ / "wal.dat";
    
    // Initial data
    ConcurrentKVStore store1;
    store1.put("init_k", "init_v");
    
    // Snapshot
    sm.save(store1.snapshot());
    
    // More ops to WAL
    WriteAheadLog wal1(wal_path.string());
    wal1.append(Command::PUT, "new_k", "new_v");
    wal1.append(Command::PUT, "init_k", "upd_v");
    
    // Recovery
    ConcurrentKVStore store2;
    if (sm.exists()) {
        store2.loadFromSnapshot(sm.load());
    }
    WriteAheadLog wal2(wal_path.string());
    wal2.replay(store2);
    
    EXPECT_EQ(store2.size(), 2);
    EXPECT_EQ(store2.get("new_k").value, "new_v");
    EXPECT_EQ(store2.get("init_k").value, "upd_v");
}

} // namespace test
} // namespace kvstore
