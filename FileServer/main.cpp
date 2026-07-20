#include "FileServer.h"

#include <fstream>
#include <iostream>
#include <jsoncpp/json/json.h>

int main()
{
    spdlog::set_level(spdlog::level::debug);
    Json::Value root;
    std::ifstream fs("config.json");
    if (!fs.is_open())
    {
        std::cerr << "Failed to open config.json" << std::endl;
        return 1;
    }

    Json::Reader reader;
    if (!reader.parse(fs, root))
    {
        std::cerr << "Failed to parse config.json" << std::endl;
        return 1;
    }

    std::string ip = root["ip"].asString();
    uint16_t port = root["FileServerPort"].asUInt();

    muduo::net::EventLoop loop;
    muduo::net::InetAddress addr(ip, port);
    FileServer server(&loop, addr);
    server.start();
    loop.loop();

    return 0;
}
