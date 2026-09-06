#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include<spdlog/spdlog.h>
#include "info.h"
#include<unistd.h>
#include<openssl/md5.h>
#include"cdc.h"
const int CHUNK_SIZE=1024*1024;
class FileManager
{
public:
    explicit FileManager(std::string baseDir,std::string blocksDir);

    bool upload(const std::string& path,
                const char* data,
                size_t len);

    bool download(const std::string& path,
                  std::vector<char>& data,uint64_t offset);

    bool remove(const std::string& path);

    bool rename(const std::string& oldName,
                const std::string& newName);

    bool createDir(const std::string& path);

    bool listFiles(const std::string& userPath,
                   std::vector<FileInfo>& files);

    bool appendFile(const std::string& filename,
                    const void* data,
                    size_t len,
                    uint64_t offset);

    bool writeBlock(const std::string& hash, const void* data, size_t len); // ./blocks/<hash>
    bool readBlock(const std::string& hash, std::vector<char>& out);
    bool removeBlock(const std::string& hash);
    // 按块清单顺序拼接,重建完整文件
    bool rebuildFile(const std::vector<BlockInfo>& blocks, const std::string& targetPath);

    bool rebuildWholeFile(const std::vector<BlockInfo>& blocks,
                                   std::string& md5, std::string& relPath);
                                   
private:
    std::string normalizePath(const std::string& path) const;
    std::string resolvePath(const std::string& relativePath) const;

    std::string baseDir_;
    std::string blocksDir_;
};

#endif