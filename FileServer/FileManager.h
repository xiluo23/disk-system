#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "info,h"

class FileManager
{
public:
    explicit FileManager(std::string baseDir = "./files");

    bool upload(const std::string& path,
                const char* data,
                size_t len);

    bool download(const std::string& path,
                  std::vector<char>& data);

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

private:
    std::string normalizePath(const std::string& path) const;
    std::string resolvePath(const std::string& relativePath) const;

    std::string baseDir_;
};

#endif