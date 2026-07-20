#ifndef LOGINSERVER_H
#define LOGINSERVER_H
#include<muduo/net/TcpServer.h>
#include<muduo/net/EventLoop.h>
#include<muduo/net/InetAddress.h>
#include<muduo/net/TcpConnection.h>
#include<muduo/base/Logging.h>
#include<muduo/net/Buffer.h>
#include<functional>
#include<spdlog/logger.h>
#include<iostream>
#include<memory>
#include<unordered_map>
#include<fstream>
#include"Login.pb.h"
#include"MySQL.h"
#include<jsoncpp/json/json.h>
using namespace std;
class LoginServer
{
public:
    LoginServer(muduo::net::EventLoop* loop, const muduo::net::InetAddress& listenAddr);
    ~LoginServer();
    void start();
    void Mylogin(const LoginRequest&,LoginResponse&);

private:
    muduo::net::TcpServer _server;
    muduo::net::EventLoop* _loop;

    void onConnection(const muduo::net::TcpConnectionPtr& conn);
    void onMessage(const muduo::net::TcpConnectionPtr& conn, muduo::net::Buffer* buf, muduo::Timestamp time);
    unique_ptr<MySQL> _mysql;
};


#endif