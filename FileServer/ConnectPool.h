#pragma once 
#include"MySQL.h"
#include<queue>
#include<mutex>
#include<condition_variable>
#include<spdlog/spdlog.h>
#include<jsoncpp/json/json.h>
#include<fstream>
#include<string>
using namespace std;

// 简单的 MySQL 连接池:
//  - 启动时预建 initialSize 个连接
//  - 用完后 InQue 归还
//  - 取连接时若临时耗尽,会阻塞等待(maxWaitSeconds)而不是裸返回 nullptr
//  - 在硬上限(initialSize)内,耗尽时按需补建,避免低峰不足
class ConnectPool {
public:
    explicit ConnectPool(int initialSize=5);
    ~ConnectPool();

    MySQL* getConnection();       // 阻塞等待;超时仍无连接才返回 nullptr
    void   InQue(MySQL* conn);    // 归还
    bool   alive() const;         // 是否还能取到连接(判空/启动检查用)

private:
    void addConnection();         // 用缓存的配置新建一个连接并入池

    queue<MySQL*> connections_;
    mutable mutex mtx_;
    condition_variable cv_;

    string host_;
    string user_;
    string password_;
    string database_;

    int maxSize_;        // 硬上限(含已借出)
    int created_;        // 已创建数(含已借出)
    bool configOk_;
};
