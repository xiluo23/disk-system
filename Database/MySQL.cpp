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

std::string MySQL::escapeString(const std::string value)
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
    // spdlog::debug("MySQL update: {}", sql);
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
    // spdlog::debug("MySQL query: {}", sql);
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
    update(sql);
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

bool MySQL::deleteFile(int userId, const std::string& fileName,const std::string&parentPath,std::vector<std::string>&storage_paths)
{
    if (!conn_)
        return false;

    std::string sql =
        "SELECT is_dir, storage_id "
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
    bool isDir=std::stoi(row[0])!=0;
    int storageId =-1;
    if(row[1])
        storageId=std::stoi(row[1]);

    mysql_free_result(res);
    if(isDir){
        // 当前目录路径
        std::string currentPath = parentPath;
        if (currentPath != "/" && !currentPath.empty())
            currentPath += "/";
        currentPath += fileName;

        // 查询子节点
        sql =
            "SELECT filename "
            "FROM user_file "
            "WHERE user_id=" + std::to_string(userId) +
            " AND parent_path='" + escapeString(currentPath) + "'";

        res = query(sql);
        if (!res)
            return false;
        std::vector<std::string>children;
        while ((row = mysql_fetch_row(res)) != nullptr)
        {
            children.emplace_back(row[0]);
        }
        mysql_free_result(res);
        for(auto&child:children){
            if (!deleteFile(userId,
                            child,
                            currentPath,
                            storage_paths))
            {
                std::cout<<"deleteFile in for children\n";
                mysql_free_result(res);
                return false;
            }
        }
        // 删除目录自身
        sql =
            "DELETE FROM user_file "
            "WHERE user_id=" + std::to_string(userId) +
            " AND parent_path='" + escapeString(parentPath) + "'" +
            " AND filename='" + escapeString(fileName) + "'";

        return update(sql);
    }
    else if(storageId!=-1){
        sql =
            "DELETE FROM user_file "
            "WHERE user_id=" + std::to_string(userId) +
            " AND parent_path='" + escapeString(parentPath) + "'" +
            " AND filename='" + escapeString(fileName) + "'";
        update(sql);
        std::string path;
        if(!getStoragePath(storageId,path)){
                std::cout<<"get storagePath fail in decreaseRefCount\n";
                return false;
        }
        if(decreaseRefCount(storageId)){
            storage_paths.emplace_back(path);
        }
    }
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


bool MySQL::getUploadTask(int clientid,const std::string md5,int& uploadSize){
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
    bool ret=update(sql);
    return ret;
}

int MySQL::getFileSize(int userId,const std::string&parentPath,const std::string&filename){
    if(!conn_)return -1;
    std::string sql=
        "SELECT filesize from user_file where user_id="+std::to_string(userId)+" AND parent_path='"
        +escapeString(parentPath)+"' AND filename='"+escapeString(filename)+"'";
    MYSQL_RES* res=query(sql);
    if(!res){
        return -1;
    }
    MYSQL_ROW row=mysql_fetch_row(res);
    if(!row)return -1;
    return std::stoi(row[0]);
}

bool MySQL::fileEOF(int userId, const std::string& md5)
{
    if(!conn_){
        return false;
    }
    std::string sql =
        "SELECT file_size, uploaded_size "
        "FROM upload_task "
        "WHERE user_id = " + std::to_string(userId) +
        " AND md5 = '" + escapeString(md5) + "'";
    MYSQL_RES*res=query(sql);
    if(!res)return false;
    MYSQL_ROW row=mysql_fetch_row(res);
    if(!row){
        mysql_free_result(res);
        return false;
    }
    bool ret=std::stoi(row[0])<=std::stoi(row[1]);
    mysql_free_result(res);
    return ret;
}

int MySQL::getUploadTaskId(int userId, const std::string& md5)
{
    if (!conn_)
    {
        return -1;
    }

    std::string sql =
        "SELECT id "
        "FROM upload_task "
        "WHERE user_id = " + std::to_string(userId) +
        " AND md5 = '" + escapeString(md5) + "'";


    MYSQL_RES* res = query(sql);
    if (res == nullptr)
    {
        return -1;
    }

    MYSQL_ROW row = mysql_fetch_row(res);

    int uploadId = -1;

    if (row != nullptr)
    {
        uploadId = std::stoi(row[0]);
    }

    mysql_free_result(res);

    return uploadId;
}
bool MySQL::insertUploadChunk(std::string upload_id,std::string chunk_index,std::string chunk_size){
    if(!conn_){
        return false;
    }
    std::string sql=
      "INSERT INTO upload_chunk("
            "upload_id,"
            "chunk_index,"
            "chunk_size,"
            "status)"
            " VALUES("
            + escapeString(upload_id) + ","
            + escapeString(chunk_index) + ","
            + escapeString(chunk_size) + ",1)";
    return update(sql);
}

bool MySQL::getFinishedChunk(int clientid,int upload_id,std::vector<int>&finishChunks){
    if(!conn_)return false;
    std::string sql =
        "SELECT chunk_index "
        "FROM upload_chunk "
        "WHERE upload_id = " + escapeString(std::to_string(upload_id)) +
        " AND status = 1 "
        "ORDER BY chunk_index ASC";
    MYSQL_RES*res=query(sql);
    if(!res)return false;
    MYSQL_ROW row=mysql_fetch_row(res);
    if(!row){
        mysql_free_result(res);
        return false;
    }
    do
    {
        finishChunks.push_back(std::stoi(row[0]));
    }while ((row = mysql_fetch_row(res)) != nullptr);

    mysql_free_result(res);
    return true;
    
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


std::string MySQL::getToken(int clientid){
    if (!conn_)
        return "";
    std::string sql = "SELECT token FROM user WHERE id = " + std::to_string(clientid) + " LIMIT 1";
    MYSQL_RES* res = query(sql);
    if (res == nullptr)
    {
        return "";
    }
    MYSQL_ROW row = mysql_fetch_row(res);
    std::string token = "";
    if (row != nullptr)
    {
        token = std::string(row[0]);
    }
    mysql_free_result(res);
    return token;
}

int MySQL::insertBlockIfAbsent(const string& hash, size_t size, int& blockId){
    if (!conn_)
        return -1;
    string path="./blocks/"+hash;
    string sql="INSERT INTO content_block(block_hash, block_size, ref_count,  storage_path) "
               "VALUES('"+escapeString(hash)+"',"+std::to_string(size)+",1,'"+escapeString(path)+"') "
               "ON DUPLICATE KEY UPDATE id=LAST_INSERT_ID(id)";
    if(!update(sql)){
        return -1;
    }
    blockId=static_cast<int>(mysql_insert_id(conn_));
    return blockId;
}


bool MySQL::updateBlockRefCount(const string& hash, int delta,bool& deleteIfZero){
    if(!conn_)
        return false;
    string sql="UPDATE content_block SET ref_count=ref_count+"+std::to_string(delta)+" WHERE block_hash="+escapeString(hash);
    if(!update(sql)){
        return false;
    }
    if(delta==-1){
        sql="SELECT ref_count FROM content_block WHERE block_hash="+escapeString(hash);
        MYSQL_RES* res=query(sql);
        deleteIfZero=false;
        if(!res)return false;
        MYSQL_ROW row=mysql_fetch_row(res);
        if(!row){
            mysql_free_result(res);
            return false;
        }
        int ref_count=std::stoi(row[0]);
        mysql_free_result(res);
        if(ref_count==0){
            deleteIfZero=true;
        }
    }
    return true;
}

bool MySQL::saveFileBlockMap(int fileId, int version, const std::vector<BlockInfo>& blocks){
    if(!conn_)
        return false;
    if(blocks.empty())
        return false;    
    // 开启事务
    if (!update("START TRANSACTION"))
        return false;
    //删除旧清单
    string del="DELETE FROM file_block_map WHERE file_id="+std::to_string(fileId)+" AND version="+std::to_string(version);
    if(!update(del)){
        update("ROLLBACK");
        return false;
    }
    
    string sql="INSERT INTO file_block_map(file_id, version, block_index, block_hash,block_size) VALUES ";
    bool first=true;
    for(auto&block:blocks){
        if(!first){
            sql+=",";
        }
        sql+="("+std::to_string(fileId)+","+std::to_string(version)+","+std::to_string(block.id)+",'"+escapeString(block.hash)+"',"+std::to_string(block.size)+")";
        first=false;
    }
    if(!update(sql)){
        update("ROLLBACK");
        return false;
    }
    if(!update("COMMIT")){
        update("ROLLBACK");
        return false;
    }
    return true;
}


std::vector<BlockInfo> MySQL::getFileBlockMap(int fileId, int version){
    if(!conn_)
        return {};
    string sql="SELECT block_index, block_hash, block_size, ref_count, storage_path FROM file_block_map WHERE file_id="+std::to_string(fileId)+" AND version="+std::to_string(version)+" ORDER BY block_index ASC";
    MYSQL_RES* res=query(sql);
    if(!res)return {};
    std::vector<BlockInfo>blocks;
    MYSQL_ROW row=nullptr;
    while((row=mysql_fetch_row(res))!=nullptr){
        BlockInfo block;
        block.id=row[0]?std::stoi(row[0]):0;
        block.hash=row[1]?row[1]:"";
        block.size=row[2]?std::stoull(row[2]):0;
        block.refCount=row[3]?std::stoull(row[3]):0;
        block.storagePath=row[4]?row[4]:"";
        blocks.push_back(block);
    }
    return blocks;
}
    
int MySQL::getLatestVersion(int fileId){
    if(!conn_)
        return -1;
    string sql="SELECT MAX(version) FROM file_block_map WHERE file_id="+std::to_string(fileId);
    MYSQL_RES* res=query(sql);
    if(!res)return -1;
    MYSQL_ROW row=mysql_fetch_row(res);
    if(!row)return -1;
    int version=row[0]?std::stoi(row[0]):-1;
    return version;
}


std::vector<std::string> MySQL::listRefHashes(int fileId, int version){
    if(!conn_)
        return {};
    string sql="SELECT DISTINCT block_hash FROM file_block_map WHERE file_id="+std::to_string(fileId)+" AND version="+std::to_string(version);
    MYSQL_RES* res=query(sql);
    if(!res)return {};
    std::vector<std::string>hashes;
    MYSQL_ROW row=nullptr;
    while((row=mysql_fetch_row(res))!=nullptr){
        hashes.push_back(row[0]?row[0]:"");
    }
    return hashes;
}

bool MySQL::updateFileStorage(int userId, int fileId,
                              const std::string& md5,
                              const std::string& storagePath,
                              uint64_t fileSize,
                              std::vector<std::string>&oldPaths 
                            )   // 归零旧文件路径
{
    if (!conn_) return false;

    // 1. 旧 storage_id
    std::string sql =
        "SELECT storage_id FROM user_file "
        "WHERE user_id = " + std::to_string(userId) +
        " AND id = " + std::to_string(fileId);
    MYSQL_RES* res = query(sql);
    if (!res) return false;
    MYSQL_ROW row = mysql_fetch_row(res);
    int oldId = -1;
    if (row && row[0]) oldId = std::stoi(row[0]);
    mysql_free_result(res);

    // 2. 整文件去重
    int storageId = -1;
    bool unchanged = false;
    if (checkMD5(md5, storageId)) {          // 已有同 md5 的物理文件
        unchanged = (storageId == oldId);    // 内容没变 → 不折腾 ref
    } else {
        if (!insertStorage(md5, storagePath, fileSize, storageId))
            return false;                    // ref=0,unchanged=false
    }

    // 3. 引用新 storage(内容变化或换 storage 才 +1)
    if (!unchanged) {
        if (!increaseRefCount(storageId)) return false;
    }

    // 4. user_file 指向新 storage
    sql =
        "UPDATE user_file SET storage_id = " + std::to_string(storageId) +
        ", filesize = " + std::to_string(fileSize) +
        " WHERE user_id = " + std::to_string(userId) +
        " AND id = " + std::to_string(fileId);
    if (!update(sql)) return false;

    // 5. 旧 storage 被本行释放;id 相同说明没换内容,不处理
    if (oldId != -1 && oldId != storageId) {
        std::string oldPath;
        if (getStoragePath(oldId, oldPath)) {
            if (decreaseRefCount(oldId))          // 归零会 DELETE file_storage 行
                oldPaths.push_back(oldPath);      // 交调用方删磁盘文件
        }
    }
    return true;
}

bool MySQL::getMissingHashes(const std::vector<std::string>& hashes, std::vector<std::string>& missingHashes){
    if(!conn_) return false;
    string sql="SELECT block_hash FROM content_block WHERE block_hash IN (";
    for(size_t i=0;i<hashes.size();++i){
        if(i>0)sql+=",";
        sql+="'"+escapeString(hashes[i])+"'";
    }
    sql+=")";
    MYSQL_RES* res=query(sql);
    if(!res)return false;
    std::unordered_set<std::string>existingHashes;
    MYSQL_ROW row=nullptr;
    while((row=mysql_fetch_row(res))!=nullptr){
        existingHashes.insert(row[0]?row[0]:"");
    }

    for(const auto& hash : hashes){
        if(existingHashes.find(hash) == existingHashes.end()){
            missingHashes.push_back(hash);
        }
    }
    return true;
}

int MySQL::getFileId(int userId,const std::string&parentPath,const std::string&filename){
    if(!conn_){
        return false;
    }
    std::string sql =
        "SELECT id FROM user_file "
        "WHERE user_id = " + std::to_string(userId) +
        " AND parent_path = '" + escapeString(parentPath) + "'" +
        " AND filename = '" + escapeString(filename) + "' LIMIT 1";
    MYSQL_RES* res = query(sql);
    if (!res) return -1;
    MYSQL_ROW row = mysql_fetch_row(res);
    int id = (row && row[0]) ? std::stoi(row[0]) : -1;
    mysql_free_result(res);
    return id;
}

bool MySQL::syncCommit(int userId, int fileId, int version,
                       const std::vector<BlockInfo>& blocks,
                       const std::string& md5, const std::string& relPath,
                       std::vector<std::string>& oldFiles,
                       std::vector<std::string>& deadBlocks)
{
    if (!conn_ || blocks.empty()) return false;
    if (!update("START TRANSACTION")) return false;

    auto fail = [&] { update("ROLLBACK"); return false; };

    // a) 旧版本清单(用于块 ref 回减)
    std::vector<BlockInfo> oldBlocks;
    if (version > 1)
        oldBlocks=getFileBlockMap(fileId, version - 1);

    // b) 写新版本清单
    if (!saveFileBlockMap(fileId, version, blocks)) return fail();

    // c) 新块 ref+1;旧版本里不在新清单的块 ref-1,归零收集
    for (const auto& b : blocks)
    {
        int id = -1;
        insertBlockIfAbsent(b.hash, b.size, id);
    }
    for (const auto& b : oldBlocks)
    {
        bool deleted=true;
        updateBlockRefCount(b.hash,-1,deleted);
        if(deleted)
            deadBlocks.push_back(b.hash);   // ref 归零,事务提交后删物理块
    }

    // d) user_file 切到新整文件(旧整文件 ref 归零 → oldFiles)
    uint64_t totalSize = 0;
    for (auto& b : blocks) totalSize += b.size;
    if (!updateFileStorage(userId, fileId, md5, relPath, totalSize, oldFiles))
        return fail();

    if (!update("COMMIT")) return fail();
    return true;
}