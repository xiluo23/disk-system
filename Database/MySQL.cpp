#include "MySQL.h"
#include <cstdint>
#include <iostream>
#include <vector>

MySQL::MySQL()
    : conn_(mysql_init(nullptr))
{
    if (!conn_)
    {
        std::cerr << "MySQL initialization failed" << std::endl;
    }
}

MySQL::~MySQL()
{
    if (conn_)
        mysql_close(conn_);
}

bool MySQL::connect(const std::string& host,
                    const std::string& user,
                    const std::string& password,
                    const std::string& database,
                    unsigned int port)
{
    if (!conn_)
    {
        conn_ = mysql_init(nullptr);
        if (!conn_)
        {
            std::cerr << "MySQL initialization failed" << std::endl;
            return false;
        }
    }

    if (mysql_real_connect(conn_,
                           host.c_str(),
                           user.c_str(),
                           password.c_str(),
                           database.c_str(),
                           port,
                           nullptr,
                           0) == nullptr)
    {
        std::cerr << mysql_error(conn_) << std::endl;
        return false;
    }

    mysql_set_character_set(conn_, "utf8mb4");
    return true;
}

std::string MySQL::escapeString(const std::string& value)
{
    if (!conn_)
        return {};

    std::vector<char> buffer(value.size() * 2 + 1);
    unsigned long escapedLength = mysql_real_escape_string(conn_, buffer.data(), value.c_str(), static_cast<unsigned long>(value.size()));
    return std::string(buffer.data(), escapedLength);
}

bool MySQL::checkMD5(const std::string md5,int&storageId){
    if (!conn_)
    {
        std::cerr << "MySQL connection is not initialized" << std::endl;
        return false;
    }
    std::string sql =
        "SELECT id "
        "FROM file_storage "
        "WHERE md5='" + escapeString(md5) + "' "
        "LIMIT 1";

    MYSQL_RES* res = query(sql);
    if (!res)
        return false;

    MYSQL_ROW row = mysql_fetch_row(res);

    bool exist = false;

    if (row && row[0])
    {
        storageId = std::stoi(row[0]);
        exist = true;
    }

    mysql_free_result(res);
    return exist;
}



bool MySQL::update(const std::string& sql)
{
    if (!conn_)
    {
        std::cerr << "MySQL connection is not initialized" << std::endl;
        return false;
    }

    if (mysql_query(conn_, sql.c_str()) != 0)
    {
        std::cerr << mysql_error(conn_) << std::endl;
        return false;
    }

    return true;
}

MYSQL_RES* MySQL::query(const std::string& sql)
{
    if (!conn_)
    {
        std::cerr << "MySQL connection is not initialized" << std::endl;
        return nullptr;
    }

    if (mysql_query(conn_, sql.c_str()) != 0)
    {
        std::cerr << mysql_error(conn_) << std::endl;
        return nullptr;
    }

    return mysql_store_result(conn_);
}

MYSQL* MySQL::getConnection()
{
    return conn_;
}

bool MySQL::queryUser(const std::string& email, User& user)
{
    if (!conn_)
        return false;

    std::string escapedEmail = escapeString(email);
    std::string sql = "SELECT id, email, password, token FROM user WHERE email = '" + escapedEmail + "' LIMIT 1";
    MYSQL_RES* res = query(sql);
    if (!res)
        return false;

    bool found = false;
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row)
    {
        found = true;
        user.id = row[0] ? std::stoi(row[0]) : 0;
        user.email = row[1] ? row[1] : std::string();
        user.password = row[2] ? row[2] : std::string();
        user.token = row[3] ? row[3] : std::string();
    }

    mysql_free_result(res);
    return found;
}

bool MySQL::insertUser(const std::string& email, const std::string& password)
{
    if (!conn_)
        return false;

    std::string escapedEmail = escapeString(email);
    std::string escapedPassword = escapeString(password);
    std::string sql = "INSERT INTO user (email, password) VALUES ('" + escapedEmail + "', '" + escapedPassword + "')";
    return update(sql);
}

bool MySQL::increaseRefCount(const int&storageID){
    if(!conn_){
        return false;
    }
    std::string sql="UPDATE file_storage set ref_count=ref_count+1 where id = "+std::to_string(storageID);
    return update(sql);
}
bool MySQL::decreaseRefCount(const int&storageId){
    if(!conn_){
        return false;
    }
    // 引用计数减一
    std::string sql =
        "UPDATE file_storage "
        "SET ref_count=ref_count-1 "
        "WHERE id=" + std::to_string(storageId);

    if (!update(sql))
        return false;

    // 查询引用计数
    sql =
        "SELECT ref_count "
        "FROM file_storage "
        "WHERE id=" + std::to_string(storageId);

    MYSQL_RES*res = query(sql);

    if (!res)
        return false;

    MYSQL_ROW row = mysql_fetch_row(res);

    if (!row)
    {
        mysql_free_result(res);
        return false;
    }
    mysql_free_result(res);
    int ref_count=std::stoi(row[0]);
    if(ref_count)return false;
    //删除文件节点
    sql =
        "DELETE FROM file_storage "
        "WHERE id=" + std::to_string(storageId);
    bool ret=update(sql);
    return true;
    
}
bool MySQL::updateToken(int id, const std::string& token)
{
    if (!conn_)
        return false;

    std::string escapedToken = escapeString(token);
    std::string sql = "UPDATE user SET token = '" + escapedToken + "' WHERE id = " + std::to_string(id);
    return update(sql);
}
bool MySQL::insertStorage(const std::string& md5,
                          const std::string& storagePath,
                          uint64_t fileSize,int&storageId)
{
    if (!conn_)
        return false;

    std::string sql =
        "INSERT INTO file_storage "
        "(md5, storage_path, file_size, ref_count, create_time) "
        "VALUES ('"
        + escapeString(md5) + "','"
        + escapeString(storagePath) + "',"
        + std::to_string(fileSize)
        + ",0,NOW())";
    if(!update(sql)){
        return false;
    }
    storageId=(static_cast<int>(mysql_insert_id(conn_)));
    return true;
}
bool MySQL::insertUserFile(int userId,
                           const std::string& fileName,
                           const std::string& parentPath,
                           uint64_t fileSize,
                           int storageId,
                           bool isDir)
{
    if (!conn_)
        return false;

    std::string sql =
        "INSERT INTO user_file "
        "(user_id, filename, parent_path, filesize, storage_id, is_dir, create_time) "
        "VALUES ("
        + std::to_string(userId)
        + ",'"
        + escapeString(fileName)
        + "','"
        + escapeString(parentPath)
        + "',"
        + std::to_string(fileSize)
        + ","
        + (isDir ? "NULL" : std::to_string(storageId))
        + ","
        + (isDir ? "1" : "0")
        + ",NOW())";
    if(!update(sql)){
        return false;
    }
    if(!increaseRefCount(storageId)){
        return false;
    }
    return true;
}

bool MySQL::deleteFile(int userId, const std::string& fileName,const std::string&parentPath,bool&deleted)
{
    if (!conn_)
        return false;

    std::string sql =
        "SELECT storage_id "
        "FROM user_file "
        "WHERE user_id=" + std::to_string(userId) +
        " AND parent_path='" + escapeString(parentPath) + "'" +
        " AND filename='" + escapeString(fileName) + "'";

    MYSQL_RES* res = query(sql);

    if (!res)
        return false;

    MYSQL_ROW row = mysql_fetch_row(res);

    if (!row)
    {
        mysql_free_result(res);
        return false;
    }

    int storageId = std::stoi(row[0]);

    mysql_free_result(res);

    // 删除用户文件记录
    sql =
        "DELETE FROM user_file "
        "WHERE user_id=" + std::to_string(userId) +
        " AND parent_path='" + escapeString(parentPath) + "'" +
        " AND filename='" + escapeString(fileName) + "'";

    if (!update(sql))
        return false;

    // 没有人引用了
    deleted=decreaseRefCount(storageId);
    return true;
}

bool MySQL::renameFile(int userId,
                       const std::string& parentPath,
                       const std::string& oldName,
                       const std::string& newName)
{
    if (!conn_)
        return false;

    std::string escapedParent = escapeString(parentPath);
    std::string escapedOld = escapeString(oldName);
    std::string escapedNew = escapeString(newName);

    // 查询是否是目录
    std::string sql =
        "SELECT is_dir FROM user_file WHERE user_id=" +
        std::to_string(userId) +
        " AND parent_path='" + escapedParent +
        "' AND filename='" + escapedOld + "'";

    MYSQL_RES* res = query(sql);

    if (!res)
        return false;


    MYSQL_ROW row = mysql_fetch_row(res);

    if (!row || !row[0])
    {
        mysql_free_result(res);
        return false;
    }

    bool isDir = std::stoi(row[0]) != 0;

    mysql_free_result(res);


    // 开启事务
    if (!update("START TRANSACTION"))
        return false;


    // 修改当前文件/目录名称
    sql =
        "UPDATE user_file SET filename='" +
        escapedNew +
        "' WHERE user_id=" +
        std::to_string(userId) +
        " AND parent_path='" +
        escapedParent +
        "' AND filename='" +
        escapedOld +
        "'";


    if (!update(sql))
    {
        update("ROLLBACK");
        return false;
    }


    // 如果是目录，需要修改所有子文件parent_path
    if (isDir)
    {
        std::string oldPath = parentPath +(parentPath.back()!='/'? "/" + oldName:oldName);
        std::string newPath = parentPath +(parentPath.back()!='/'? "/" + newName:newName);

        sql =
            "UPDATE user_file SET parent_path=REPLACE(parent_path,'"
            + escapeString(oldPath)
            + "','"
            + escapeString(newPath)
            + "') WHERE user_id="
            + std::to_string(userId)
            + " AND parent_path LIKE '"
            + escapeString(oldPath)
            + "%'";


        if (!update(sql))
        {
            update("ROLLBACK");
            return false;
        }
    }


    if (!update("COMMIT"))
    {
        update("ROLLBACK");
        return false;
    }


    return true;
}

bool MySQL::listFiles(int userId,
                      const std::string& parentPath,
                      std::vector<FileInfo>& files)
{
    if (!conn_)
        return false;

    std::string sql =
        "SELECT filename, filesize, is_dir "
        "FROM user_file "
        "WHERE user_id = " + std::to_string(userId) +
        " AND parent_path = '" + escapeString(parentPath) + "' "
        "ORDER BY is_dir DESC, filename ASC";

    MYSQL_RES* res = query(sql);
    if (!res)
        return false;

    MYSQL_ROW row;

    while ((row = mysql_fetch_row(res)) != nullptr)
    {
        FileInfo info;

        info.name = row[0] ? row[0] : "";

        info.size = row[1]
                        ? std::stoull(row[1])
                        : 0;

        info.isDir = row[2]
                        ? std::stoi(row[2]) != 0
                        : false;

        files.push_back(info);
    }

    mysql_free_result(res);

    return true;
}

bool MySQL::getStoragePath(int storageId, std::string& storagePath)
{
    if (!conn_)
        return false;

    std::string sql =
        "SELECT storage_path "
        "FROM file_storage "
        "WHERE id = " + std::to_string(storageId);

    MYSQL_RES* res = query(sql);
    if (!res)
        return false;

    MYSQL_ROW row = mysql_fetch_row(res);

    if (!row || !row[0])
    {
        mysql_free_result(res);
        return false;
    }

    storagePath = row[0];

    mysql_free_result(res);

    return true;
}

int MySQL::getStorageID(int userId,
                        const std::string& parentPath,
                        const std::string& filename)
{
    if (!conn_)
        return -1;

    std::string sql =
        "SELECT storage_id "
        "FROM user_file "
        "WHERE user_id=" + std::to_string(userId) +
        " AND parent_path='" + escapeString(parentPath) + "'" +
        " AND filename='" + escapeString(filename) + "'" +
        " LIMIT 1";

    MYSQL_RES* res = query(sql);
    if (!res)
        return -1;

    MYSQL_ROW row = mysql_fetch_row(res);

    int storageId = -1;

    if (row && row[0])
        storageId = std::stoi(row[0]);

    mysql_free_result(res);

    return storageId;
}


bool MySQL::getUploadTask(int clientid,const std::string&md5,int& uploadSize){
    if(!conn_){
        return false;
    }
    std::string sql=
        "SELECT uploaded_size FROM upload_task WHERE md5='"+escapeString(md5)+"' AND user_id="+std::to_string(clientid);
    MYSQL_RES*res=query(sql);
    if(!res){
        return false;
    }
    MYSQL_ROW row=mysql_fetch_row(res);
    if(!row)return false;
    uploadSize=std::stoi(row[0]);
    mysql_free_result(res);
    return true;
}
bool MySQL::insertUploadTask(int clientid,const std::string& md5,const std::string& filename,const std::string& parentpath,const std::string&storagePath,int filesize){
    if(!conn_){
        return false;
    }
    std::string sql =
        "INSERT INTO upload_task "
        "(user_id, md5, filename, parent_path, storage_path, file_size, uploaded_size) "
        "VALUES ("
        + std::to_string(clientid) + ", '"
        + escapeString(md5) + "', '"
        + escapeString(filename) + "', '"
        + escapeString(parentpath) + "', '"
        + escapeString(storagePath) + "', "
        + std::to_string(filesize) + ", 0)";
    return update(sql);
}


bool MySQL::updateUploadTask(int clientid,const std::string& md5,int upload_size){
    if (!conn_)
        return false;

    std::string sql =
        "UPDATE upload_task "
        "SET uploaded_size = uploaded_size+" + std::to_string(upload_size) +
        " WHERE user_id = " + std::to_string(clientid) +
        " AND md5 = '" + escapeString(md5) + "'";

    return update(sql);
}


bool MySQL::deleteUploadTask(int clientid,const std::string& md5)
{
    if (!conn_)
        return false;

    std::string sql =
        "DELETE FROM upload_task "
        "WHERE user_id = " + std::to_string(clientid) +
        " AND md5 = '" + escapeString(md5) + "'";

    return update(sql);
}