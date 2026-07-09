#ifndef MYSERVER_H
#define MYSERVER_H
#include<muduo/net/TcpServer.h>
#include<muduo/net/EventLoop.h>
#include<muduo/net/InetAddress.h>
#include<muduo/net/TcpConnection.h>
#include<muduo/base/Logging.h>
#include<muduo/net/Buffer.h>
#include<functional>
#include"MyRSA.h"
#include<iostream>
#include"proto/Request.pb.h"
#include"proto/Response.pb.h"
#include"MyAES.h"
#include<memory>
#include<unordered_map>
using namespace std;
class MyServer
{
public:
    MyServer(muduo::net::EventLoop* loop, const muduo::net::InetAddress& listenAddr);
    ~MyServer();
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

    void onConnection(const muduo::net::TcpConnectionPtr& conn);
    void onMessage(const muduo::net::TcpConnectionPtr& conn, muduo::net::Buffer* buf, muduo::Timestamp time);
};


#endif