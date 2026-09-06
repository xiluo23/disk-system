#ifndef MYSQL_H
#define MYSQL_H

#include <mysql/mysql.h>
#include <cstdint>
#include <string>
#include <vector>
#include"info.h"
#include<spdlog/spdlog.h>
#include<unordered_set>
using namespace std;
class MySQL
{
public:
    MySQL();
    ~MySQL();

    // 连接数据库
    bool connect(const std::string& host,
                 const std::string& user,
                 const std::string& password,
                 const std::string& database,
                 unsigned int port = 3306);

    // 执行增删改
    bool update(const std::string& sql);

    // 执行查询
    MYSQL_RES* query(const std::string& sql);

    // 获取底层连接
    MYSQL* getConnection();

    bool queryUser(const std::string& email, User& user);

    bool insertUser(const std::string& email,
                    const std::string& password);

    bool updateToken(int id,
                     const std::string& token);

    bool deleteFile(int userId, const std::string& fileName,const std::string&parentPath,std::vector<std::string>&storage_paths);

    bool renameFile(int userId,
                       const std::string& parentPath,
                       const std::string& oldName,
                       const std::string& newName);

    bool listFiles(int userId,
                    const std::string& parentPath,
                    std::vector<FileInfo>& files);

    bool checkMD5(const std::string md5,int&);
        
    bool insertStorage(const std::string& md5,
                          const std::string& storagePath,
                          uint64_t fileSize,int&);
    bool increaseRefCount(const int&);
    bool decreaseRefCount(const int&);
    bool insertUserFile(int userId,
                           const std::string& fileName,
                           const std::string& parentPath,
                           uint64_t fileSize,
                           int storageId,
                           bool isDir);
    bool getStoragePath(int storageId, std::string& storagePath);
    int getStorageID(int userId,const std::string& parentPath,const std::string& filename);
    int getFileSize(int userId,const std::string&parentPath,const std::string&filename);
    int getFileId(int userId,const std::string&parentPath,const std::string&filename);

    bool getUploadTask(int clientid,const std::string md5,int& uploadSize);
    bool insertUploadTask(int clientid,const std::string& md5,const std::string& filename,const std::string& path,const std::string&storagePath,int filesize);
    bool updateUploadTask(int clientid,const std::string& md5,int upload_size);
    bool deleteUploadTask(int clientid,const std::string& md5);

    bool getFinishedChunk(int clientid,int upload_id,std::vector<int>&finishChunks);
    bool fileEOF(int userId, const std::string& md5);
    int getUploadTaskId(int userId, const std::string& md5);
    bool insertUploadChunk(std::string upload_id,std::string chunk_index,std::string chunk_size);
    std::string getToken(int clientid);
    
    /**
     * 块入池，存在直接返回已有id
     */
    int insertBlockIfAbsent(const string& hash, size_t size, int& blockId);

    /**
     * 修改块引用计数
     */
    bool updateBlockRefCount(const string& hash, int delta,bool& deleteIfZero);

    /**
     * 写文件块清单
     */
    bool saveFileBlockMap(int fileId, int version, const std::vector<BlockInfo>& blocks);

    //读某版块清单
    std::vector<BlockInfo> getFileBlockMap(int fileId, int version);
    
    int getLatestVersion(int fileId);

    //列出某版用到的所有 hash
    std::vector<std::string> listRefHashes(int fileId, int version);

    bool updateFileStorage(int userId, int fileId, const std::string& md5, const std::string& storagePath, uint64_t fileSize,std::vector<std::string>& oldPaths);

    //得得服务端没有的hash块
    bool getMissingHashes(const std::vector<std::string>& hashes, std::vector<std::string>& missingHashes);

    //
    bool syncCommit(int userId, int fileId, int version,
                       const std::vector<BlockInfo>& blocks,
                       const std::string& md5, const std::string& relPath,
                       std::vector<std::string>& oldFiles,
                       std::vector<std::string>& deadBlocks);


private:
    
    std::string escapeString(const std::string value);
    MYSQL* conn_;
};

#endif