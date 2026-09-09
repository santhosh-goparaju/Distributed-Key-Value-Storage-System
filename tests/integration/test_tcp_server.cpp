#include <gtest/gtest.h>
#include "networking/tcp_server.h"
#include "networking/tcp_connection.h"
#include "storage/concurrent_kv_store.h"
#include <thread>
#include <chrono>

using namespace kvstore;

class TcpServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        store_ = std::make_unique<ConcurrentKVStore>();
        server_ = std::make_unique<TcpServer>("127.0.0.1", 0, 4);
        server_->setHandler([this](const Request& req) -> Response {
            switch (req.command) {
                case Command::PUT: return store_->put(req.key, req.value);
                case Command::GET: return store_->get(req.key);
                case Command::DELETE: return store_->del(req.key);
                case Command::PING: return {Status::OK, "PONG"};
                default: return {Status::INVALID_REQUEST, ""};
            }
        });
        server_thread_ = std::thread([this] { server_->start(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // wait for server to start
    }
    void TearDown() override {
        server_->stop();
        if (server_thread_.joinable()) server_thread_.join();
    }
    std::unique_ptr<ConcurrentKVStore> store_;
    std::unique_ptr<TcpServer> server_;
    std::thread server_thread_;
    
    Response sendRequest(const Request& req) {
        auto conn_opt = TcpConnection::connect("127.0.0.1", server_->port());
        EXPECT_TRUE(conn_opt.has_value());
        if (!conn_opt) return {Status::ERROR, ""};
        TcpConnection conn = std::move(*conn_opt);
        
        std::vector<uint8_t> req_data = protocol::serializeRequest(req);
        EXPECT_TRUE(protocol::writeFrame(conn.fd(), req_data));
        
        std::vector<uint8_t> resp_data;
        EXPECT_TRUE(protocol::readFrame(conn.fd(), resp_data));
        
        auto resp_opt = protocol::deserializeResponse(resp_data.data(), resp_data.size());
        EXPECT_TRUE(resp_opt.has_value());
        return resp_opt.value_or(Response{Status::ERROR, ""});
    }
};

TEST_F(TcpServerTest, PutAndGetResponse) {
    Request put_req{Command::PUT, "key1", "val1"};
    Response put_resp = sendRequest(put_req);
    EXPECT_EQ(put_resp.status, Status::OK);
    
    Request get_req{Command::GET, "key1", ""};
    Response get_resp = sendRequest(get_req);
    EXPECT_EQ(get_resp.status, Status::OK);
    EXPECT_EQ(get_resp.value, "val1");
}

TEST_F(TcpServerTest, DeleteAndNotFound) {
    Request put_req{Command::PUT, "key2", "val2"};
    sendRequest(put_req);
    
    Request del_req{Command::DELETE, "key2", ""};
    Response del_resp = sendRequest(del_req);
    EXPECT_EQ(del_resp.status, Status::OK);
    
    Request get_req{Command::GET, "key2", ""};
    Response get_resp = sendRequest(get_req);
    EXPECT_EQ(get_resp.status, Status::NOT_FOUND);
}

TEST_F(TcpServerTest, PingPong) {
    Request ping_req{Command::PING, "", ""};
    Response ping_resp = sendRequest(ping_req);
    EXPECT_EQ(ping_resp.status, Status::OK);
    EXPECT_EQ(ping_resp.value, "PONG");
}

TEST_F(TcpServerTest, SequentialRequestsOnSameConnection) {
    auto conn_opt = TcpConnection::connect("127.0.0.1", server_->port());
    ASSERT_TRUE(conn_opt.has_value());
    TcpConnection conn = std::move(*conn_opt);
    
    auto sendAndReceive = [&conn](const Request& req) -> Response {
        std::vector<uint8_t> req_data = protocol::serializeRequest(req);
        protocol::writeFrame(conn.fd(), req_data);
        std::vector<uint8_t> resp_data;
        protocol::readFrame(conn.fd(), resp_data);
        return protocol::deserializeResponse(resp_data.data(), resp_data.size()).value();
    };
    
    Response r1 = sendAndReceive({Command::PUT, "seq1", "v1"});
    EXPECT_EQ(r1.status, Status::OK);
    
    Response r2 = sendAndReceive({Command::GET, "seq1", ""});
    EXPECT_EQ(r2.status, Status::OK);
    EXPECT_EQ(r2.value, "v1");
}

TEST_F(TcpServerTest, ConcurrentClients) {
    auto worker = [this](int id) {
        std::string key = "c_key" + std::to_string(id);
        std::string val = "c_val" + std::to_string(id);
        
        auto conn_opt = TcpConnection::connect("127.0.0.1", server_->port());
        ASSERT_TRUE(conn_opt.has_value());
        TcpConnection conn = std::move(*conn_opt);
        
        Request req{Command::PUT, key, val};
        std::vector<uint8_t> req_data = protocol::serializeRequest(req);
        EXPECT_TRUE(protocol::writeFrame(conn.fd(), req_data));
        
        std::vector<uint8_t> resp_data;
        EXPECT_TRUE(protocol::readFrame(conn.fd(), resp_data));
        auto resp = protocol::deserializeResponse(resp_data.data(), resp_data.size());
        ASSERT_TRUE(resp.has_value());
        EXPECT_EQ(resp->status, Status::OK);
    };
    
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(worker, i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // Verify one of them
    Request get_req{Command::GET, "c_key2", ""};
    Response get_resp = sendRequest(get_req);
    EXPECT_EQ(get_resp.status, Status::OK);
    EXPECT_EQ(get_resp.value, "c_val2");
}
