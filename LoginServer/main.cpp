#include"LoginServer.h"
#include<iostream>
#include<jsoncpp/json/json.h>
#include<fstream>
using namespace std;


int main()
{
    Json::Value root;
    ifstream fs("config.json");
    if (!fs.is_open()) {
        std::cerr << "Failed to open JSON file" << std::endl;
        return 1;
    }
    Json::Reader reader;
    if(!reader.parse(fs,root)){
        std::cerr << "Failed to parse config.json" << std::endl;
        return 1;
    }
    string ip=root["ip"].asString();
    uint16_t port=root["LoginServerPort"].asUInt();

    muduo::net::EventLoop loop;
    muduo::net::InetAddress addr(ip,port);
    LoginServer server(&loop,addr);
    server.start();
    loop.loop();
    
    return 0;
}