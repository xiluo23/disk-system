#ifndef MYSQL_H
#define MYSQL_H

#include <mysql/mysql.h>
#include <cstdint>
#include <string>
#include <vector>
#include"info,h"
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

    bool deleteFile(int userId, const std::string& fileName,const std::string&parentPath,bool&deleted);

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



    bool getUploadTask(int clientid,const std::string&md5,int& uploadSize);
    bool insertUploadTask(int clientid,const std::string& md5,const std::string& filename,const std::string& path,const std::string&storagePath,int filesize);
    bool updateUploadTask(int clientid,const std::string& md5,int upload_size);
    bool deleteUploadTask(int clientid,const std::string& md5);
private:
    
    std::string escapeString(const std::string& value);
    MYSQL* conn_;
};

#endif