#pragma once 
#include"MySQL.h"
#include<queue>
#include<mutex>
#include<spdlog/spdlog.h>
#include<jsoncpp/json/json.h>
#include<fstream>
using namespace std;
class ConnectPool {
public:
    explicit ConnectPool(int initialSize=5);
    ~ConnectPool();

    void addConnection(const std::string& host,
                 const std::string& user,
                 const std::string& password,
                 const std::string& database);
    MySQL* getConnection();
    void InQue(MySQL* conn);

private:
    queue<MySQL*> connections;
    mutex mtx;
};