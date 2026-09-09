cmake_minimum_required(VERSION 3.16)
project(kv-store VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Wpedantic)
include_directories(${PROJECT_SOURCE_DIR}/include)

find_package(Threads REQUIRED)

add_library(kvstore_common INTERFACE)
target_include_directories(kvstore_common INTERFACE ${PROJECT_SOURCE_DIR}/include)

add_library(kvstore_storage STATIC
    src/storage/kv_store.cpp
    src/storage/concurrent_kv_store.cpp
)
target_link_libraries(kvstore_storage kvstore_common Threads::Threads)

add_library(kvstore_protocol STATIC
    src/protocol/protocol.cpp
)
target_link_libraries(kvstore_protocol kvstore_common)

add_library(kvstore_networking STATIC
    src/networking/thread_pool.cpp
    src/networking/tcp_connection.cpp
    src/networking/tcp_server.cpp
)
target_link_libraries(kvstore_networking kvstore_common kvstore_protocol Threads::Threads)

add_library(kvstore_persistence STATIC
    src/persistence/wal.cpp
    src/persistence/snapshot.cpp
)
target_link_libraries(kvstore_persistence kvstore_common kvstore_storage)

add_library(kvstore_cluster STATIC
    src/cluster/consistent_hash.cpp
    src/cluster/cluster_manager.cpp
    src/cluster/replication_manager.cpp
    src/cluster/health_monitor.cpp
)
target_link_libraries(kvstore_cluster kvstore_common kvstore_storage kvstore_networking kvstore_persistence)

add_executable(kv-server src/server/server_main.cpp)
target_link_libraries(kv-server kvstore_storage kvstore_networking kvstore_persistence kvstore_cluster)

add_executable(kv-client src/client/cli_client.cpp)
target_link_libraries(kv-client kvstore_networking kvstore_protocol)

add_executable(kv-benchmark benchmarks/benchmark_client.cpp)
target_link_libraries(kv-benchmark kvstore_networking kvstore_protocol Threads::Threads)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)
enable_testing()

add_executable(test_kv_store tests/unit/test_kv_store.cpp)
target_link_libraries(test_kv_store kvstore_storage GTest::gtest_main Threads::Threads)
add_test(NAME test_kv_store COMMAND test_kv_store)

add_executable(test_tcp_server tests/integration/test_tcp_server.cpp)
target_link_libraries(test_tcp_server kvstore_storage kvstore_networking GTest::gtest_main Threads::Threads)
add_test(NAME test_tcp_server COMMAND test_tcp_server)

add_executable(test_recovery tests/integration/test_recovery.cpp)
target_link_libraries(test_recovery kvstore_storage kvstore_persistence GTest::gtest_main Threads::Threads)
add_test(NAME test_recovery COMMAND test_recovery)

add_executable(test_node_failure tests/failure/test_node_failure.cpp)
target_link_libraries(test_node_failure kvstore_cluster kvstore_persistence GTest::gtest_main Threads::Threads)
add_test(NAME test_node_failure COMMAND test_node_failure)
