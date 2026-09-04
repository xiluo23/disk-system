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
#include"ThreadPool.h"
#include"ConnectPool.h"
#include"Guard.h"
#include <fstream>
#include <iostream>
#include <jsoncpp/json/json.h>
#include <sys/ipc.h>
using namespace std;

class FileServer
{
public:
    FileServer(muduo::net::EventLoop* loop, const muduo::net::InetAddress& listenAddr);
    ~FileServer();
    void start();
    bool verifyToken(const string&,const string&);
    string calcMD5(const unsigned char*data,size_t len);
    void dispatch(const FileRequest& req,FileResponse& rsp,const muduo::net::TcpConnectionPtr&conn);
    bool checkIsExist(const std::string& md5, int& storageId);
private:
    muduo::net::TcpServer _server;
    muduo::net::EventLoop* _loop;

    void onConnection(const muduo::net::TcpConnectionPtr& conn);
    void onMessage(const muduo::net::TcpConnectionPtr& conn, muduo::net::Buffer* buf, muduo::Timestamp time);
    
    void handleUpload(const FileRequest& req,FileResponse&rsp);

    void handleDownload(const FileRequest& req,const muduo::net::TcpConnectionPtr&conn);

    void handleDelete(const FileRequest& req,FileResponse&rsp);

    void handleList(const FileRequest& req,FileResponse&rsp);

    void handleMkdir(const FileRequest& req,FileResponse&rsp);

    void handleRename(const FileRequest& req,FileResponse&rsp);

    void handleUploadCheck(const FileRequest& req,FileResponse&rsp);

    void handleDownloadCheck(const FileRequest& req,FileResponse&rsp);

    // SYNC_CHECK: 收客户端块清单 → 返回服务端缺失块
    void handleSyncCheck(const FileRequest&, FileResponse&);
    
    // SYNC_UPLOAD: 收单个新块(密文 data + block_hash) → 写块池,ref++
    void handleSyncUpload(const FileRequest&, FileResponse&);
    
    // SYNC_COMMIT: 收完整块清单 → 写 file_block_map
    //              → rebuildFile 重建用户文件
    //              → 更新 user_file(storage_id),旧块 ref--,空则删块
    void handleSyncCommit(const FileRequest&, FileResponse&);

    bool encryptResponseData(const std::string& clientId, FileResponse& rsp);

    unique_ptr<SecKeyShm> _secShm;
    unique_ptr<FileManager> _fileManager;
    unique_ptr<ConnectPool> _connectPool;
    unique_ptr<ThreadPool> _filePool;
};


#endif