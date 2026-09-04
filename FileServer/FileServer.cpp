#include "FileServer.h"



// 初始化文件服务器，绑定 TCP 事件回调并准备共享内存与数据库连接。
FileServer::FileServer(muduo::net::EventLoop* loop, const muduo::net::InetAddress& listenAddr)
    : _server(loop, listenAddr, "FileServer"), _loop(loop)
{
    _server.setConnectionCallback(std::bind(&FileServer::onConnection, this, std::placeholders::_1));
    _server.setMessageCallback(std::bind(&FileServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    _server.setThreadNum(4);
    key_t key = ftok("/tmp", 1);
    if (key == -1)
    {
        perror("ftok");
    }
    _filePool=make_unique<ThreadPool>(4);
    _filePool->start();

    _secShm = make_unique<SecKeyShm>(key);
    _secShm->init();

    _fileManager = make_unique<FileManager>();

    _connectPool = make_unique<ConnectPool>(4);

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

void FileServer::dispatch(const FileRequest& req,FileResponse& rsp,const muduo::net::TcpConnectionPtr&conn)
{
    switch(req.type())
    {
    case UPLOAD_FILE:
        handleUpload(req, rsp);
        break;

    case DOWNLOAD_FILE:
        handleDownload(req,conn);
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
    case DOWNLOAD_CHECK:
        handleDownloadCheck(req,rsp);
        break;
    case SYNC_CHECK:
        handleSyncCheck(req,rsp);
        break;
    case SYNC_UPLOAD:
        handleSyncUpload(req,rsp);
        break;
    case SYNC_COMMIT:
        handleSyncCommit(req,rsp);
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
        dispatch(req, rsp,conn);
        if(req.type()==DOWNLOAD_FILE)break;
        string packet=Codec::encode(rsp);
        conn->send(packet);
    }
}
void FileServer::handleDownloadCheck(const FileRequest& req,FileResponse&rsp){
    rsp.set_type(DOWNLOAD_CHECK);
    if(!verifyToken(req.token(),req.clientid()))
    {
        rsp.set_status(false);
        return;
    }
    Guard guard(_connectPool.get());
    MySQL* _mysql=guard.getConnection();
    
    int fileSize=_mysql->getFileSize(stoi(req.clientid()),req.path(),req.filename());
    if(fileSize==-1){
        rsp.set_status(false);
        return;
    }
    rsp.set_status(true);
    rsp.set_filesize(fileSize);
    auto file=rsp.add_files();
    file->set_filename(req.filename());
    spdlog::debug("send downloadcheck");
}

void FileServer::handleUploadCheck(const FileRequest& req,FileResponse&rsp){
    rsp.set_type(UPLOAD_CHECK);
    if (!verifyToken(req.token(), req.clientid()))
    {
        spdlog::warn("verifyToken fail");
        rsp.set_status(false);
        rsp.set_message("Invalid token");
        return;
    }
    int storageId=-1;
    Guard guard(_connectPool.get());
    MySQL* _mysql=guard.getConnection();
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
        std::vector<int>finishChunks;
        int clientid=std::stoi(req.clientid());

        _mysql->getFinishedChunk(clientid,_mysql->getUploadTaskId(clientid,req.md5()),finishChunks);
        for(auto&index:finishChunks){
            rsp.add_finished_chunks(index);
        }
        spdlog::debug("check upload task for clientid: {}, md5: {}", req.clientid(), req.md5());
        rsp.set_upload_id(_mysql->getUploadTaskId(std::stoi(req.clientid()),req.md5()));
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
    _mysql->insertUploadTask(std::stoi(req.clientid()),req.md5(),req.filename(),req.path(),storagePath,req.filesize());
    rsp.set_upload_id(_mysql->getUploadTaskId(std::stoi(req.clientid()),req.md5()));
    rsp.set_status(false);
    rsp.set_offset(0);
    rsp.set_message("New upload");
}
// 校验客户端 token 是否与数据库中保存的 token 一致。
bool FileServer::verifyToken(const string& token, const string& clientid)
{
    Guard guard(_connectPool.get());
    MySQL* _mysql=guard.getConnection();
    if (token.empty() || clientid.empty() || !_mysql || !_mysql->getConnection())
    {
        spdlog::info("token or clientif mysql error");
        return false;
    }
    
    return _mysql->getToken(std::stoi(clientid)) == token;
}
//检验文件是否已存在
bool FileServer::checkIsExist(const std::string& md5, int& storageId)
{
    Guard guard(_connectPool.get());
    MySQL* _mysql=guard.getConnection();
    return _mysql->checkMD5(md5,storageId);
}
string FileServer::calcMD5(const unsigned char*data,size_t len){
    unsigned char md[MD5_DIGEST_LENGTH];
    MD5(data,len,md);
    char buf[33];
    // std::cout<<"my MD5:";

    for(int i = 0; i < MD5_DIGEST_LENGTH; ++i)
    {
        // printf("%02x",buf[i]);
        sprintf(buf + i * 2, "%02x", md[i]);
    }
    // std::cout<<'\n';
    buf[32] = '\0';
    return string(buf);
}
// 处理上传请求：先验证身份，再解密文件内容，最后写入文件系统。
void FileServer::handleUpload(const FileRequest& req, FileResponse& rsp)
{
    Guard guard(_connectPool.get());
    MySQL* _mysql=guard.getConnection();
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
    if(req.chunk_md5()!=calcMD5(plain.data(),plain.size())){
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
    if(!_mysql->updateUploadTask(stoi(req.clientid()),req.md5(),req.chunk_size())){
        spdlog::warn("{} updateUploadTask fail",req.filename());
        rsp.set_status(false);
        return ;
    }
    if(!_mysql->insertUploadChunk(to_string(req.upload_id()),to_string(req.chunk_index()),to_string(req.chunk_size()))){
            spdlog::warn("{} insertUploadChunk fail",req.filename());
            rsp.set_status(false);
            return ;
    }
    if(_mysql->fileEOF(stoi(req.clientid()),req.md5())){
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
    }
    rsp.set_status(true);
    rsp.set_message("Upload success");
}

void sendResponse(FileResponse&rsp,const muduo::net::TcpConnectionPtr&conn){
    std::string packet =Codec::encode(rsp);
        // 回sub reactor
    conn->getLoop()->queueInLoop(
        [conn,packet]()
        {
            conn->send(packet);
            spdlog::debug("send packet size {}",packet.size());
        });
}
// 处理下载请求：读取文件内容并放入响应 data 中。
void FileServer::handleDownload(const FileRequest& req,const muduo::net::TcpConnectionPtr&conn)
{
    _filePool->submit([this,req,conn](){
        FileResponse rsp;
        rsp.set_type(DOWNLOAD_FILE);
        if(!verifyToken(req.token(),req.clientid())){
            rsp.set_status(false);
            sendResponse(rsp,conn);
            return ;
        }
        // 从共享内存获取AES密钥
        SecKeyInfo* info = _secShm->find(req.clientid().c_str());
        if (info == nullptr)
        {
            rsp.set_status(false);
            rsp.set_message("AES key not found");
            sendResponse(rsp,conn);
            return ;
        }
        // 初始化AES
        MyAES aes;
        aes.setKey(reinterpret_cast<const unsigned char*>(info->secKey));
        aes.setIV(reinterpret_cast<const unsigned char*>(req.iv().data()));
        // 读取文件
        unique_ptr<MySQL>mysql=make_unique<MySQL>();
        Json::Value root;
        std::ifstream fs("config.json");
        if (!fs.is_open())
        {
            spdlog::warn("config.json not found, file server will run without DB auth");
            rsp.set_status(false);
            sendResponse(rsp,conn);
            return;
        }
        Json::Reader reader;
        if (!reader.parse(fs, root))
        {
            spdlog::warn("Failed to parse config.json");
            rsp.set_status(false);
            sendResponse(rsp,conn);
            return ;
        }
        if (!mysql->connect(root["host"].asString(),
                             root["user"].asString(),
                             root["password"].asString(),
                             root["database"].asString()))
        {
            spdlog::warn("Failed to connect database for FileServer token validation");
            rsp.set_status(false);
            sendResponse(rsp,conn);
            return ;
        }
        int storageID=mysql->getStorageID(stoi(req.clientid()),req.path(),req.filename());
        std::string storagePath;
        if(!mysql->getStoragePath(storageID,storagePath)){
            spdlog::warn("getStoragePath fail");
            rsp.set_status(false);
            rsp.set_message("File");
            sendResponse(rsp,conn);
            return ;
        }
        std::vector<char> data;
        rsp.set_offset(req.offset());
        bool ok=_fileManager->download(storagePath,data,req.offset());
        if(ok)
        {
            rsp.set_status(true);
            rsp.set_md5(calcMD5((const unsigned char*)data.data(),data.size()));
            // AES加密
            std::vector<unsigned char> cipher;
            if (!aes.encrypt(reinterpret_cast<const unsigned char*>(data.data()),data.size(),cipher))
            {
                rsp.set_status(false);
                rsp.set_message("AES encrypt failed");
                sendResponse(rsp,conn);
                return;
            }
            rsp.set_message("Download success");
            FileItem*file=rsp.add_files();
            file->set_filename(req.filename());
            file->set_filesize(req.filesize());
            file->set_isdir(false);
            rsp.set_data(cipher.data(), cipher.size());
        }
        else
        {
            rsp.set_status(false);
        }
        rsp.set_eof(data.size()<CHUNK_SIZE);
        spdlog::debug("send data size {}",data.size());
        sendResponse(rsp,conn);
    });
}
// 删除指定的客户端文件。
void FileServer::handleDelete(const FileRequest& req, FileResponse& rsp)
{
    rsp.set_type(DELETE_FILE);
    Guard guard(_connectPool.get());
    MySQL* _mysql=guard.getConnection();
    if (!verifyToken(req.token(), req.clientid()))
    {
        rsp.set_status(false);
        rsp.set_message("Invalid token");
        return;
    }
    std::vector<std::string>storage_paths;
    if (!_mysql->deleteFile(std::stoi(req.clientid()), req.filename(),req.path(),storage_paths))
    {
        spdlog::warn("Delete file metadata failed for {}", req.filename());
        rsp.set_status(false);
        rsp.set_message("DeleteFile failed");
        return ;
    }
    for(auto&path:storage_paths){
        if (!_fileManager->remove(path))
        {
            spdlog::warn("{} fileManager remove fail",path);
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
    Guard guard(_connectPool.get());
    MySQL* _mysql=guard.getConnection();
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
    Guard guard(_connectPool.get());
    MySQL* _mysql=guard.getConnection();
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
    Guard guard(_connectPool.get());
    MySQL* _mysql=guard.getConnection();
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

void FileServer::handleSyncCheck(const FileRequest&req, FileResponse&rsp){
    rsp.set_type(SYNC_CHECK);
    Guard guard(_connectPool.get());
    MySQL* _mysql=guard.getConnection();
    if (!verifyToken(req.token(), req.clientid()))
    {
        rsp.set_status(false);
        rsp.set_message("Invalid token");
        return;
    }
     // 1. 收客户端发来的块哈希清单(它本地已算好的新块清单)
    std::vector<std::string> hashes;
    for (const auto& h : req.block_hashes())
        if (!h.empty()) hashes.push_back(h);
    if (hashes.empty())
    {
        rsp.set_status(false);
        rsp.set_message("empty block list");
        return;
    }

    // 2. 查 content_block:返回服务端没有的块 → 客户端只传这些
    std::vector<std::string> missing;
    if (!_mysql->getMissingHashes(hashes, missing))
    {
        rsp.set_status(false);
        rsp.set_message("query failed");
        return;
    }
    for (auto& h : missing)
        rsp.add_missing_hashes(h);

    // 3. 返回文件当前版本(不存在 = 全新上传,version 0)
    int fileId = _mysql->getFileId(
        std::stoi(req.clientid()), req.path(), req.filename());
    if (fileId != -1)
        rsp.set_upload_id(_mysql->getLatestVersion(fileId));
    else
        rsp.set_upload_id(0);

    rsp.set_status(true);
}
    
 
void FileServer::handleSyncUpload(const FileRequest&req, FileResponse&rsp){
    rsp.set_type(SYNC_UPLOAD)
    Guard guard(_connectPool.get());
    MySQL* _mysql=guard.getConnection();
    if (!verifyToken(req.token(), req.clientid()))
    {
        rsp.set_status(false);
        rsp.set_message("Invalid token");
        return;
    }
    const std::string& hash = req.block_hash();
    if (hash.empty())
    {
        rsp.set_status(false);
        rsp.set_message("empty block_hash");
        return;
    }

    // 1. 取 AES 会话密钥(沿用密钥协商体系)
    SecKeyInfo* info = _secShm->find(req.clientid().c_str());
    if (info == nullptr)
    {
        rsp.set_status(false);
        rsp.set_message("AES key not found");
        return;
    }

    // 2. 解密
    MyAES aes;
    aes.setKey(reinterpret_cast<const unsigned char*>(info->secKey));
    aes.setIV(reinterpret_cast<const unsigned char*>(req.iv().data()));
    std::vector<unsigned char> plain;
    if (!aes.decrypt(reinterpret_cast<const unsigned char*>(req.data().data()),
                     req.data().size(), plain))
    {
        rsp.set_status(false);
        rsp.set_message("AES decrypt failed");
        return;
    }

    // 3. 内容寻址校验:明文 BLAKE2s 必须等于客户端声称的 block_hash
    if (blake2s_hex(plain.data(), plain.size()) != hash)
    {
        spdlog::warn("block hash mismatch, refuse");
        rsp.set_status(false);
        rsp.set_message("block hash mismatch");
        return;
    }

    // 4. 写磁盘块池(已存在则幂等成功)
    if (!_fileManager->writeBlock(hash, plain.data(), plain.size()))
    {
        rsp.set_status(false);
        rsp.set_message("write block failed");
        return;
    }

    // 5. DB 登记:只保证行存在、返回 id;ref 计数留给 syncCommit 统一 +1
    int blockId = -1;
    if (_mysql->insertBlockIfAbsent(hash, plain.size(), blockId) == -1)
    {
        rsp.set_status(false);
        rsp.set_message("db insert block failed");
        return;
    }

    rsp.set_status(true);
    rsp.set_message("SyncUpload success");
}
    
void FileServer::handleSyncCommit(const FileRequest&req, FileResponse&rsp){
    rsp.set_type(SYNC_COMMIT);
    Guard guard(_connectPool.get());
    MySQL* _mysql=guard.getConnection();
    if (!verifyToken(req.token(), req.clientid()))
    { rsp.set_status(false); rsp.set_message("Invalid token"); return; }

    // 1. 解析清单(下标即 block_index)
    std::vector<BlockInfo> blocks;
    int n = req.block_hashes_size();
    if (n == 0 || n != req.block_sizes_size())
    { rsp.set_status(false); rsp.set_message("bad manifest"); return; }
    for (int i = 0; i < n; ++i)
    {
        BlockInfo b;
        b.index = i;
        b.hash  = req.block_hashes(i);
        b.size  = req.block_sizes(i);
        blocks.push_back(b);
    }

    int userId = std::stoi(req.clientid());
    int fileId = _mysql->getFileId(userId, req.path(), req.filename());
    if (fileId == -1)
    { rsp.set_status(false); rsp.set_message("file not found"); return; }

    // 2. 乐观锁:客户端带它看到的当前版本,防覆盖别人刚提交的新版本
    int cur = _mysql->getLatestVersion(fileId);
    if (req.version() != cur)
    { rsp.set_status(false); rsp.set_message("version conflict"); return; }
    int version = cur + 1;

    // 3. 磁盘就绪:拼整文件 + 流式算 md5 + 发布成 sync/<md5>
    std::string md5, relPath;                 // relPath = "sync/<md5>"
    if (!_fileManager->rebuildWholeFile(blocks, md5, relPath))
    { rsp.set_status(false); rsp.set_message("rebuild failed"); return; }

    // 4. DB 大事务
    std::vector<std::string> oldFiles;        // 旧整文件(ref归零)待删
    std::vector<std::string> deadBlocks;      // 旧版本块(ref归零)待删
    if (!_mysql->syncCommit(userId, fileId, version, blocks,
                            md5, relPath, oldFiles, deadBlocks))
    { rsp.set_status(false); rsp.set_message("commit failed"); return; }

    // 5. 提交后清理(DB 已一致,失败只留孤儿文件)
    for (auto& p : oldFiles)  _fileManager->remove(p);
    for (auto& h : deadBlocks) _fileManager->removeBlock(h);

    rsp.set_status(true);
    rsp.set_upload_id(version);
}

