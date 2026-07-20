#ifndef SECMNGSERVER_H
#define SECMNGSERVER_H
#include<muduo/net/TcpServer.h>
#include<muduo/net/EventLoop.h>
#include<muduo/net/InetAddress.h>
#include<muduo/net/TcpConnection.h>
#include<muduo/base/Logging.h>
#include<muduo/net/Buffer.h>
#include<functional>
#include<spdlog/logger.h>
#include"MyRSA.h"
#include<iostream>
#include"Request.pb.h"
#include"Response.pb.h"
#include"MyAES.h"
#include<memory>
#include<unordered_map>
#include"SecKeyShm.h"
using namespace std;
class SecMngServer
{
public:
    SecMngServer(muduo::net::EventLoop* loop, const muduo::net::InetAddress& listenAddr);
    ~SecMngServer();
    void start();
    
    void handle_keyagree(const Request&req,Response&rsp);
    void handle_keycheck(const Request&req,Response&rsp);
    void handle_keyrevoke(const Request&req,Response&rsp);

    bool check_sign(const Request&req);
private:
    muduo::net::TcpServer _server;
    muduo::net::EventLoop* _loop;

    std::unordered_map<std::string,unique_ptr<MyRSA>>_rsaMap;
    std::unordered_map<std::string,unique_ptr<MyAES>>_aesMap; 
    
    unique_ptr<SecKeyShm> _secshm;

    void onConnection(const muduo::net::TcpConnectionPtr& conn);
    void onMessage(const muduo::net::TcpConnectionPtr& conn, muduo::net::Buffer* buf, muduo::Timestamp time);
};


#endif