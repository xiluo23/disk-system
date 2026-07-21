#ifndef FILESERVER_H
#define FILESERVER_H
#include<muduo/net/TcpServer.h>
#include<muduo/net/EventLoop.h>
#include<muduo/net/InetAddress.h>
#include<muduo/net/TcpConnection.h>
#include<muduo/base/Logging.h>
#include<muduo/net/Buffer.h>
#include<functional>
#include<spdlog/spdlog.h>
#include<iostream>
#include<memory>
#include<unordered_map>
#include<fstream>
#include"MySQL.h"
#include<openssl/md5.h>
#include<jsoncpp/json/json.h>
#include"SecKeyShm.h"
#include"File.pb.h"
#include"FileManager.h"
#include"MyAES.h"
#include"Codec.h"
using namespace std;

class FileServer
{
public:
    FileServer(muduo::net::EventLoop* loop, const muduo::net::InetAddress& listenAddr);
    ~FileServer();
    void start();
    bool verifyToken(const string&,const string&);
    string calcMD5(const unsigned char*data,size_t len);
    void dispatch(const FileRequest& req,FileResponse& rsp);
    bool checkIsExist(const std::string& md5, int& storageId);
private:
    muduo::net::TcpServer _server;
    muduo::net::EventLoop* _loop;

    void onConnection(const muduo::net::TcpConnectionPtr& conn);
    void onMessage(const muduo::net::TcpConnectionPtr& conn, muduo::net::Buffer* buf, muduo::Timestamp time);
    
    void handleUpload(const FileRequest& req,FileResponse&rsp);

    void handleDownload(const FileRequest& req,FileResponse&rsp);

    void handleDelete(const FileRequest& req,FileResponse&rsp);

    void handleList(const FileRequest& req,FileResponse&rsp);

    void handleMkdir(const FileRequest& req,FileResponse&rsp);

    void handleRename(const FileRequest& req,FileResponse&rsp);

    void handleUploadCheck(const FileRequest& req,FileResponse&rsp);

    void handleDownloadCheck(const FileRequest& req,FileResponse&rsp);

    bool encryptResponseData(const std::string& clientId, FileResponse& rsp);

    unique_ptr<SecKeyShm> _secShm;
    unique_ptr<FileManager> _fileManager;
    unique_ptr<MySQL> _mysql;
};


#endif