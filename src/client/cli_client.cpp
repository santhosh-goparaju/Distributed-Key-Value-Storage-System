#include "common/types.h"
#include "networking/tcp_connection.h"
#include "protocol/protocol.h"
#include <iostream>
#include <sstream>
#include <string>
#include <cctype>

using namespace kvstore;

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    uint16_t port = 7000;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) host = argv[++i];
        else if (arg == "--port" && i + 1 < argc) port = std::stoi(argv[++i]);
    }
    
    auto conn_opt = TcpConnection::connect(host, port);
    if (!conn_opt) {
        std::cerr << "Failed to connect to " << host << ":" << port << "\n";
        return 1;
    }
    
    TcpConnection conn = std::move(*conn_opt);
    std::cout << "Connected to " << host << ":" << port << "\n";
    std::cout << "Type HELP for commands.\n";
    
    std::string line;
    while (true) {
        std::cout << "kv> ";
        if (!std::getline(std::cin, line)) {
            break; // EOF
        }
        
        if (line.empty()) continue;
        
        std::istringstream iss(line);
        std::string cmd_str;
        iss >> cmd_str;
        
        // uppercase cmd
        for (auto& c : cmd_str) c = std::toupper(c);
        
        if (cmd_str == "QUIT" || cmd_str == "EXIT") {
            break;
        } else if (cmd_str == "HELP") {
            std::cout << "Commands:\n"
                      << "  PUT <key> <value>\n"
                      << "  GET <key>\n"
                      << "  DELETE <key> or DEL <key>\n"
                      << "  PING\n"
                      << "  QUIT or EXIT\n";
            continue;
        }
        
        Request req;
        if (cmd_str == "PUT") {
            req.command = Command::PUT;
            iss >> req.key;
            std::getline(iss, req.value);
            // strip leading space from value
            if (!req.value.empty() && req.value[0] == ' ') req.value = req.value.substr(1);
        } else if (cmd_str == "GET") {
            req.command = Command::GET;
            iss >> req.key;
        } else if (cmd_str == "DELETE" || cmd_str == "DEL") {
            req.command = Command::DELETE;
            iss >> req.key;
        } else if (cmd_str == "PING") {
            req.command = Command::PING;
        } else {
            std::cout << "Unknown command: " << cmd_str << "\n";
            continue;
        }
        
        std::vector<uint8_t> req_data = protocol::serializeRequest(req);
        if (!protocol::writeFrame(conn.fd(), req_data)) {
            std::cerr << "Error sending request. Disconnected.\n";
            break;
        }
        
        std::vector<uint8_t> resp_data;
        if (!protocol::readFrame(conn.fd(), resp_data)) {
            std::cerr << "Error receiving response. Disconnected.\n";
            break;
        }
        
        auto resp_opt = protocol::deserializeResponse(resp_data.data(), resp_data.size());
        if (!resp_opt) {
            std::cerr << "Failed to parse response.\n";
            continue;
        }
        
        Response resp = *resp_opt;
        std::cout << "Status: " << statusToString(resp.status) << "\n";
        if (!resp.value.empty()) {
            std::cout << "Value: " << resp.value << "\n";
        }
    }
    
    conn.close();
    return 0;
}
