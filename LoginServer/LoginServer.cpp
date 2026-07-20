#include "LoginServer.h"
#include <chrono>
#include <fstream>
#include <spdlog/spdlog.h>


std::string generateToken(int userId)
{
    auto now = std::chrono::system_clock::now();
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return std::to_string(userId) + "_" + std::to_string(milliseconds);
}


LoginServer::LoginServer(muduo::net::EventLoop* loop, const muduo::net::InetAddress& listenAddr): 
    _server(loop, listenAddr, "LoginServer"),
      _loop(loop)
{
    _server.setConnectionCallback(std::bind(&LoginServer::onConnection, this, std::placeholders::_1));
    _server.setMessageCallback(std::bind(&LoginServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    _server.setThreadNum(2); // 设置线程数为2

    _mysql=make_unique<MySQL>();
    Json::Value root;
    ifstream fs("config.json");
    if (!fs.is_open()) {
        std::cerr << "Failed to open JSON file" << std::endl;
        return ;
    }
    Json::Reader reader;
    if(!reader.parse(fs,root)){
        std::cerr << "Failed to parse config.json" << std::endl;
        return ;
    }
    if(!_mysql->connect(root["host"].asString(),root["user"].asString(),root["password"].asString(),root["database"].asString())){
        std::cerr<<"fail to connect database\n";
        return ;
    }

}

LoginServer::~LoginServer(){
}
    
void LoginServer::start(){
    _server.start();
}

void LoginServer::onConnection(const muduo::net::TcpConnectionPtr& conn){
    if (conn->connected())
    {
        std::cout << "New Connection: "
                  << conn->peerAddress().toIpPort()
                  << std::endl;
    }
    else
    {
        std::cout << "Connection Closed: "
                  << conn->peerAddress().toIpPort()
                  << std::endl;
    }
}

void LoginServer::onMessage(const muduo::net::TcpConnectionPtr& conn, muduo::net::Buffer* buf, muduo::Timestamp time)
{
    string data=buf->retrieveAllAsString();
    LoginRequest req;
    if(!req.ParseFromString(data)){
        perror("ParseFromString fail");
        return ;
    }
    LoginResponse rsp;
    Mylogin(req,rsp);
    conn->send(rsp.SerializeAsString());
}

void LoginServer::Mylogin(const LoginRequest& req, LoginResponse& rsp)
{
    // 1. 查询邮箱是否存在
    User user;
    bool exist = _mysql->queryUser(req.email(), user);

    if (!exist)
    {
        // 2. 不存在 -> 自动注册
        if (!_mysql->insertUser(req.email(), req.password()))
        {
            rsp.set_status(false);
            rsp.set_message("Register failed");
            return;
        }

        // 查询刚插入的用户
        _mysql->queryUser(req.email(), user);
    }
    else
    {
        // 3. 已存在，验证密码
        if (user.password != req.password())
        {
            rsp.set_status(false);
            rsp.set_message("Password error");
            return;
        }
    }

    // 4. 登录成功
    rsp.set_status(true);
    rsp.set_message("Login success");

    rsp.set_clientid(std::to_string(user.id));

    std::string token = generateToken(user.id);
    if(!_mysql->updateToken(user.id,token)){
        rsp.set_status(false);
        spdlog::info("updateToken fail");
    }
    rsp.set_token(token);
}