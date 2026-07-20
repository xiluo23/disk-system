#include "FileManager.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

namespace fs = std::filesystem;

// 构造文件管理器，指定文件根目录，默认使用 ./files。
FileManager::FileManager(std::string baseDir)
    : baseDir_(std::move(baseDir))
{
    if (baseDir_.empty())
    {
        baseDir_ = "./files";
    }

    std::error_code ec;
    fs::create_directories(baseDir_, ec);
}

// 将传入路径规范化，禁止使用 .. 逃逸到根目录之外。
std::string FileManager::normalizePath(const std::string& path) const
{
    if (path.empty())
    {
        return {};
    }

    fs::path normalized;
    std::stringstream ss(path);
    std::string part;
    while (std::getline(ss, part, '/'))
    {
        if (part.empty() || part == ".")
        {
            continue;
        }

        if (part == "..")
        {
            return {};
        }

        normalized /= part;
    }

    return normalized.generic_string();
}

// 将相对路径映射到文件存储根目录下的实际物理路径。
std::string FileManager::resolvePath(const std::string& relativePath) const
{
    fs::path base = fs::absolute(baseDir_);
    if (relativePath.empty())
    {
        return base.string();
    }

    std::string normalized = normalizePath(relativePath);
    if (normalized.empty())
    {
        return base.string();
    }

    return (base / normalized).lexically_normal().string();
}

// 将数据写入指定路径，覆盖原文件内容。
bool FileManager::upload(const std::string& path, const char* data, size_t len)
{
    if (data == nullptr && len != 0)
    {
        return false;
    }

    std::string resolved = resolvePath(path);
    std::error_code ec;
    fs::create_directories(fs::path(resolved).parent_path(), ec);

    std::ofstream out(resolved, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        return false;
    }

    if (len != 0)
    {
        out.write(data, static_cast<std::streamsize>(len));
    }

    return out.good();
}

// 读取指定文件内容到内存缓冲区。
bool FileManager::download(const std::string& path, std::vector<char>& data)
{
    std::string resolved = resolvePath(path);
    std::ifstream in(resolved, std::ios::binary);
    if (!in)
    {
        return false;
    }

    data.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

// 删除指定文件或目录。
bool FileManager::remove(const std::string& path)
{
    return fs::remove(resolvePath(path));
}

// 重命名文件或目录。
bool FileManager::rename(const std::string& oldName, const std::string& newName)
{
    std::string oldPath = resolvePath(oldName);
    std::string newPath = resolvePath(newName);
    std::error_code ec;
    fs::create_directories(fs::path(newPath).parent_path(), ec);
    fs::rename(oldPath, newPath, ec);
    return !ec;
}

// 创建指定目录（包含父目录）。
bool FileManager::createDir(const std::string& path)
{
    std::error_code ec;
    return fs::create_directories(resolvePath(path), ec);
}

// 列出指定目录下的文件和子目录信息。
bool FileManager::listFiles(const std::string& userPath, std::vector<FileInfo>& files)
{
    files.clear();

    std::string resolved = resolvePath(userPath);
    std::error_code ec;
    if (!fs::exists(resolved, ec) || !fs::is_directory(resolved, ec))
    {
        return false;
    }

    for (const auto& entry : fs::directory_iterator(resolved, ec))
    {
        FileInfo info;
        info.name = entry.path().filename().string();
        info.path = entry.path().lexically_normal().string();
        info.isDir = entry.is_directory(ec);
        if (entry.is_regular_file(ec))
        {
            info.size = static_cast<std::uint64_t>(fs::file_size(entry.path(), ec));
        }
        else{
            info.size=0;
        }
        files.push_back(info);
    }

    return true;
}

// 追加写入文件内容，适用于分块上传场景。
bool FileManager::appendFile(const std::string& storagePath, const void* data, size_t len,uint64_t offset)
{
    if (data == nullptr && len != 0)
    {
        return false;
    }

    std::string resolved = resolvePath(storagePath);
    std::error_code ec;
    fs::create_directories(fs::path(resolved).parent_path(), ec);
    std::fstream file(
        resolved,
        std::ios::binary |
        std::ios::in |
        std::ios::out);

    // 文件不存在，先创建
    if (!file.is_open())
    {
        std::ofstream create(resolved, std::ios::binary);
        create.close();

        file.open(resolved,
                  std::ios::binary |
                  std::ios::in |
                  std::ios::out);

        if (!file.is_open())
            return false;
    }

    file.seekp(offset);

    file.write(static_cast<const char*>(data),
               static_cast<std::streamsize>(len));

    return file.good();
    

    

    
}
