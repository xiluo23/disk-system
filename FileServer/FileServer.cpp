#include "FileServer.h"

#include <fstream>
#include <iostream>
#include <jsoncpp/json/json.h>
#include <sys/ipc.h>

// 初始化文件服务器，绑定 TCP 事件回调并准备共享内存与数据库连接。
FileServer::FileServer(muduo::net::EventLoop* loop, const muduo::net::InetAddress& listenAddr)
    : _server(loop, listenAddr, "FileServer"),
      _loop(loop)
{
    _server.setConnectionCallback(std::bind(&FileServer::onConnection, this, std::placeholders::_1));
    _server.setMessageCallback(std::bind(&FileServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    _server.setThreadNum(4);

    key_t key = ftok("/tmp", 1);
    if (key == -1)
    {
        perror("ftok");
    }

    _secShm = make_unique<SecKeyShm>(key);
    _secShm->init();

    _fileManager = make_unique<FileManager>("./files");
    _mysql = make_unique<MySQL>();

    Json::Value root;
    std::ifstream fs("config.json");
    if (!fs.is_open())
    {
        spdlog::warn("config.json not found, file server will run without DB auth");
        return;
    }

    Json::Reader reader;
    if (!reader.parse(fs, root))
    {
        spdlog::warn("Failed to parse config.json");
        return;
    }

    if (!_mysql->connect(root["host"].asString(),
                         root["user"].asString(),
                         root["password"].asString(),
                         root["database"].asString()))
    {
        spdlog::warn("Failed to connect database for FileServer token validation");
    }
}

FileServer::~FileServer() {}

// 启动 TCP 服务。
void FileServer::start()
{
    _server.start();
}

// 处理新连接建立或断开。
void FileServer::onConnection(const muduo::net::TcpConnectionPtr& conn)
{
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

void FileServer::dispatch(const FileRequest& req,FileResponse& rsp)
{
    switch(req.type())
    {
    case UPLOAD_FILE:
        handleUpload(req, rsp);
        break;

    case DOWNLOAD_FILE:
        handleDownload(req, rsp);
        break;

    case DELETE_FILE:
        handleDelete(req, rsp);
        break;

    case LIST_FILE:
        handleList(req, rsp);
        break;

    case MKDIR:
        handleMkdir(req, rsp);
        break;

    case RENAME_FILE:
        handleRename(req, rsp);
        break;

    case UPLOAD_CHECK:
        handleUploadCheck(req,rsp);
        break;
    default:
        rsp.set_status(false);
        rsp.set_message("Unknown cmd");
        break;
    }
}

// 解析客户端发来的 FileRequest，并根据请求类型调用对应处理函数。
void FileServer::onMessage(const muduo::net::TcpConnectionPtr& conn,
                           muduo::net::Buffer* buf,
                           muduo::Timestamp time)
{
    while (true)
    {
        if (buf->readableBytes() < 4)
            return;

        uint32_t len;
        memcpy(&len, buf->peek(), 4);
        len = ntohl(len);

        if (buf->readableBytes() < len + 4)
            return;

        buf->retrieve(4);

        std::string body(buf->peek(), len);
        buf->retrieve(len);

        FileRequest req;
        if (!req.ParseFromString(body))
        {
            spdlog::error("Parse protobuf failed");
            continue;
        }

        FileResponse rsp;
        dispatch(req, rsp);
        string packet=Codec::encode(rsp);
        conn->send(packet);
    }
}

void FileServer::handleUploadCheck(const FileRequest& req,FileResponse&rsp){
    rsp.set_type(UPLOAD_CHECK);
    rsp.set_status(false);
    if (!verifyToken(req.token(), req.clientid()))
    {
        spdlog::warn("verifyToken fail");
        rsp.set_status(false);
        rsp.set_message("Invalid token");
        return;
    }
    int storageId=-1;
    //该文件已存在
    if(checkIsExist(req.md5(),storageId)){
        if(storageId==-1||!_mysql->insertUserFile(stoi(req.clientid()),req.filename(),req.path(),req.filesize(),storageId,false)){
            spdlog::warn("fail to insert UserFile");
            rsp.set_status(false);
            rsp.set_message("fali to insert UserFile");
            return;
        }
        rsp.set_status(true);
        return ;
    }
    //之前上传过
    int uploadedSize = 0;
    if (_mysql->getUploadTask(std::stoi(req.clientid()),req.md5(),uploadedSize)){
        rsp.set_status(false);
        rsp.set_message("Resume upload");
        rsp.set_offset(uploadedSize);
        return;
    }
    //新建上传任务
    std::string storagePath = req.clientid()+req.path();
    if(storagePath.back()!='/'){
        storagePath+="/";
    }
    storagePath+=req.filename();

    if (!_mysql->insertUploadTask(
            std::stoi(req.clientid()),
            req.md5(),
            req.filename(),
            req.path(),
            storagePath,
            req.filesize()))
    {
        rsp.set_status(false);
        rsp.set_message("Create upload task failed");
        return;
    }

    rsp.set_status(true);
    rsp.set_offset(0);
    rsp.set_message("New upload");
}
// 校验客户端 token 是否与数据库中保存的 token 一致。
bool FileServer::verifyToken(const string& token, const string& clientid)
{
    if (token.empty() || clientid.empty() || !_mysql || !_mysql->getConnection())
    {
        spdlog::info("token or clientif mysql error");
        return false;
    }
    std::string sql = "SELECT token FROM user WHERE id = " + clientid + " LIMIT 1";
    MYSQL_RES* res = _mysql->query(sql);
    if (!res)
    {
        spdlog::info("mysql query is empty");
        return false;
    }

    bool isValid = false;
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row && row[0])
    {
        isValid = std::string(row[0]) == token;
    }

    mysql_free_result(res);
    return isValid;
}
//检验文件是否已存在
bool FileServer::checkIsExist(const std::string& md5, int& storageId)
{
    return _mysql->checkMD5(md5,storageId);
}

bool FileServer::verifyMd5(const string&md5,const unsigned char*data,size_t len){
    unsigned char md[MD5_DIGEST_LENGTH];
    MD5(data,len,md);
    char buf[33];
    for(int i = 0; i < MD5_DIGEST_LENGTH; ++i)
    {
        sprintf(buf + i * 2, "%02x", md[i]);
    }

    buf[32] = '\0';
    return string(buf)==md5;
}
// 处理上传请求：先验证身份，再解密文件内容，最后写入文件系统。
void FileServer::handleUpload(const FileRequest& req, FileResponse& rsp)
{
    rsp.set_type(UPLOAD_FILE);
    rsp.set_eof(false);
    if (!verifyToken(req.token(), req.clientid()))
    {
        spdlog::warn("verifyToken fail");
        rsp.set_status(false);
        rsp.set_message("Invalid token");
        return;
    }
    SecKeyInfo* info = _secShm->find(req.clientid().c_str());
    if (info == nullptr)
    {
        spdlog::warn("get SecKeyInfo fail");
        rsp.set_status(false);
        rsp.set_message("AES key not found");
        return;
    }

    MyAES aes;
    aes.setKey(reinterpret_cast<const unsigned char*>(info->secKey));
    aes.setIV(reinterpret_cast<const unsigned char*>(req.iv().data()));

    std::vector<unsigned char> plain;

    if (!aes.decrypt(reinterpret_cast<const unsigned char*>(req.data().data()),req.data().size(),plain))
    {
        spdlog::warn("decrypt fail");
        rsp.set_status(false);
        rsp.set_message("AES decrypt failed");
        return;
    }
    if(!verifyMd5(req.chunk_md5(),plain.data(),plain.size())){
        spdlog::warn("verifyMd5 fail");
        rsp.set_status(false);
        rsp.set_message("verfiyMD5 fail");
        return;
    }
    std::string storagePath = req.clientid()+req.path();
    if(storagePath.back()!='/'){
        storagePath+="/";
    }
    storagePath+=req.filename();
    if (!_fileManager->appendFile(storagePath, plain.data(), plain.size(),req.offset()))
    {
        spdlog::warn("{} appendFile fail",req.filename());
        rsp.set_status(false);
        rsp.set_message("Write file failed");
        return;
    }
    if(req.eof()){
        int storageId=-1;
        if(!_mysql->insertStorage(req.md5(),storagePath,req.filesize(),storageId)){
            spdlog::warn("{} insert storage fail",req.filename());
            rsp.set_status(false);
            rsp.set_message("fail");
            return;
        }
        if (!_mysql->insertUserFile(stoi(req.clientid()),req.filename(),req.path(),req.filesize(),storageId,false))
        {
            spdlog::warn("Insert Userfile failed for {}", req.filename());
            rsp.set_status(false);
            rsp.set_message("fail to insert UserFile");
            return ;
        }
        if(!_mysql->deleteUploadTask(stoi(req.clientid()),req.md5())){
            spdlog::warn("{} deleteUploadTask faile",req.filename());
            rsp.set_status(false);
            return ;
        }
        rsp.set_eof(true);
    }
    else{
        if(!_mysql->updateUploadTask(stoi(req.clientid()),req.md5(),req.chunk_size())){
            spdlog::warn("{} updateUploadTask fail",req.filename());
            rsp.set_status(false);
            return ;
        }
    }
    rsp.set_chunk_size(req.chunk_size());
    rsp.set_status(true);
    rsp.set_message("Upload success");
}

// 处理下载请求：读取文件内容并放入响应 data 中。
void FileServer::handleDownload(const FileRequest& req, FileResponse& rsp)
{
    rsp.set_type(DOWNLOAD_FILE);

    if (!verifyToken(req.token(), req.clientid()))
    {
        rsp.set_status(false);
        rsp.set_message("Invalid token");
        return;
    }

    // 从共享内存获取AES密钥
    SecKeyInfo* info = _secShm->find(req.clientid().c_str());
    if (info == nullptr)
    {
        rsp.set_status(false);
        rsp.set_message("AES key not found");
        return;
    }

    // 初始化AES
    MyAES aes;
    aes.setKey(reinterpret_cast<const unsigned char*>(info->secKey));
    aes.setIV(reinterpret_cast<const unsigned char*>(req.iv().data()));

    // 读取文件
    std::vector<char> plain;
    int storageID=_mysql->getStorageID(stoi(req.clientid()),req.path(),req.filename());
    std::string storagePath;
    if(!_mysql->getStoragePath(storageID,storagePath)){
        spdlog::warn("getStoragePath fail");
        rsp.set_status(false);
        rsp.set_message("File");
        return;
    }

    if (!_fileManager->download(storagePath, plain))
    {
        spdlog::warn("download fail");
        rsp.set_status(false);
        rsp.set_message("File not found");
        return;
    }

    // AES加密
    std::vector<unsigned char> cipher;
    if (!aes.encrypt(
            reinterpret_cast<const unsigned char*>(plain.data()),
            plain.size(),
            cipher))
    {
        rsp.set_status(false);
        rsp.set_message("AES encrypt failed");
        return;
    }
    rsp.set_status(true);
    rsp.set_message("Download success");
    FileItem*file=rsp.add_files();
    file->set_filename(req.filename());
    file->set_filesize(req.filesize());
    file->set_isdir(false);
    rsp.set_data(cipher.data(), cipher.size());
}
// 删除指定的客户端文件。
void FileServer::handleDelete(const FileRequest& req, FileResponse& rsp)
{
    rsp.set_type(DELETE_FILE);

    if (!verifyToken(req.token(), req.clientid()))
    {
        rsp.set_status(false);
        rsp.set_message("Invalid token");
        return;
    }
    int storageId=_mysql->getStorageID(stoi(req.clientid()),req.path(),req.filename());
    std::string storagePath;
    if(!_mysql->getStoragePath(storageId,storagePath)){
        rsp.set_status(false);
        rsp.set_message("getStoragePath fail");
        return;
    }
    
    bool deleted=false;
    if (!_mysql->deleteFile(std::stoi(req.clientid()), req.filename(),req.path(),deleted))
    {
        spdlog::warn("Delete file metadata failed for {}", req.filename());
        rsp.set_status(false);
        rsp.set_message("DeleteFile failed");
        return ;
    }
    if(deleted){
        if (!_fileManager->remove(storagePath))
        {
            spdlog::warn("fileManager remove fail");
            rsp.set_status(false);
            rsp.set_message("Delete failed");
            return;
        }
    }
    rsp.set_status(true);
    rsp.set_message("Delete success");
}

// 列出指定目录下的文件和子目录。
void FileServer::handleList(const FileRequest& req, FileResponse& rsp)
{
    rsp.set_type(LIST_FILE);

    if (!verifyToken(req.token(), req.clientid()))
    {
        rsp.set_status(false);
        rsp.set_message("Invalid token");
        return;
    }

    std::vector<FileInfo> files;

    if (!_mysql->listFiles(
            std::stoi(req.clientid()),
            req.path(),
            files))
    {
        rsp.set_status(false);
        rsp.set_message("List failed");
        return;
    }
    for (const auto& file : files)
    {
        auto* item = rsp.add_files();

        item->set_filename(file.name);
        item->set_filesize(file.size);
        item->set_isdir(file.isDir);
    }
    rsp.set_status(true);
    rsp.set_message("List success");
}

// 为指定客户端创建目录。
void FileServer::handleMkdir(const FileRequest& req, FileResponse& rsp)
{
    rsp.set_type(MKDIR);

    if (!verifyToken(req.token(), req.clientid()))
    {
        rsp.set_status(false);
        rsp.set_message("Invalid token");
        return;
    }

    if (!_mysql->insertUserFile(
            std::stoi(req.clientid()),
            req.filename(),
            req.path(),
            0,
            -1,
            true))
    {
        rsp.set_status(false);
        rsp.set_message("Create dir failed");
        return;
    }
    rsp.set_status(true);
    rsp.set_message("Create dir success");
}

// 重命名指定文件或目录。
void FileServer::handleRename(const FileRequest& req, FileResponse& rsp)
{
    rsp.set_type(RENAME_FILE);

    if (!verifyToken(req.token(), req.clientid()))
    {
        rsp.set_status(false);
        rsp.set_message("Invalid token");
        return;
    }

    std::string newName(req.data().begin(), req.data().end());

    if (!_mysql->renameFile(
            std::stoi(req.clientid()),
            req.path(),
            req.filename(),
            newName))
    {
        rsp.set_status(false);
        rsp.set_message("Rename failed");
        return;
    }

    rsp.set_status(true);
    rsp.set_message("Rename success");
}