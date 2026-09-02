#include "ConnectPool.h"

ConnectPool::ConnectPool(int initialSize) {
    Json::Value root;
    std::ifstream fs("config.json");
    if (!fs.is_open())
    {
        spdlog::warn("config.json not found, file server will run without DB auth");
        return;
    }

    Json::Reader reader;
    if (!reader.parse(fs, root))
    {
        spdlog::warn("Failed to parse config.json");
        return;
    }
    string host = root["host"].asString();
    string user = root["user"].asString();
    string password = root["password"].asString();
    string database = root["database"].asString();

    for (int i = 0; i < initialSize; ++i) {
        addConnection(host, user, password, database);
    }
    spdlog::info("ConnectPool initialized with {} connections", initialSize);
}
ConnectPool::~ConnectPool(){
    while(!connections.empty()){
        MySQL* conn = connections.front();
        connections.pop();
        delete conn;
    }
}

void ConnectPool::addConnection(const std::string& host,
                 const std::string& user,
                 const std::string& password,
                 const std::string& database) {
    std::lock_guard<std::mutex> lock(mtx);
    MySQL* conn = new MySQL();
    if (!conn->connect(host, user, password, database)) {
        spdlog::error("Failed to connect to database for ConnectPool");
        delete conn;
        return;
    }
    connections.push(conn);
}

MySQL* ConnectPool::getConnection() {
    std::lock_guard<std::mutex> lock(mtx);
    // Implementation for getting a connection
    if (!connections.empty()) {
        MySQL* conn = connections.front();
        connections.pop();
        return conn;
    }
    return nullptr;
}

void ConnectPool::InQue(MySQL* conn){
    std::lock_guard<std::mutex> lock(mtx);
    connections.push(conn);
}