#include "FileManager.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

namespace fs = std::filesystem;

// 构造文件管理器，指定文件根目录，默认使用 ./files。
FileManager::FileManager(std::string baseDir,std::string blocksDir)
    : baseDir_(std::move(baseDir)), blocksDir_(std::move(blocksDir))
{
    if (baseDir.empty())
    {
        baseDir_ = "./files";
    }
    if(blocksDir.empty()){
        blocksDir_="./blocks"
    }
    std::error_code ec;
    fs::create_directories(baseDir_, ec);
    fs::create_directories(blocksDir_, ec);
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
bool FileManager::download(const std::string& path,std::vector<char>& data,uint64_t offset)
{
    std::string resolved = resolvePath(path);


    std::ifstream in(
        resolved,
        std::ios::binary);

    if(!in)
        return false;

    uint64_t fileSize =
        fs::file_size(resolved);

    if(offset >= fileSize)
    {
        data.clear();
        return true;
    }

    in.seekg(offset,std::ios::beg);

    size_t readSize =
        std::min(
            (uint64_t)CHUNK_SIZE,
            fileSize-offset);

    data.resize(readSize);

    in.read(
        data.data(),
        readSize);

    data.resize(
        in.gcount());

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

bool FileManager::writeBlock(const std::string& hash, const void* data, size_t len) // ./blocks/<hash>
{
    if (hash.empty()) return false;
    if (data == nullptr && len != 0) return false;

    std::error_code ec;
    // 块目录:./blocks

    std::string finalPath = (fs::path(blockDir) / hash).string();

    // 内容寻址:同 hash 必同内容,已存在直接成功(并发安全的关键)
    if (fs::exists(finalPath, ec))
        return true;

    // 先写唯一临时文件，多线程并发写，避免多个线程同时写同一个文件，导致文件内容不一致。
    std::string tmpPath = finalPath + ".tmp." + std::to_string(::getpid());
    {
        std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        if (len != 0)
            out.write(static_cast<const char*>(data),
                      static_cast<std::streamsize>(len));
        out.flush();
        if (!out.good()) { out.close(); fs::remove(tmpPath, ec); return false; }
    }

    // rename 原子发布:读者永远看不到半截文件
    fs::rename(tmpPath, finalPath, ec);
    if (ec)
    {
        // Windows 下目标已存在时 rename 会失败,但内容相同,等价成功
        if (fs::exists(finalPath, ec)) { fs::remove(tmpPath, ec); return true; }
        fs::remove(tmpPath, ec);
        return false;
    }
    return true;

}
bool FileManager::readBlock(const std::string& hash, std::vector<char>& out)
{
    out.clear();
    if (hash.empty()) return false;
    std::string path = (fs::path(blocksDir_)  / hash).string();
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    in.seekg(0, std::ios::end);
    std::streamoff sz = in.tellg();
    in.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(sz));
    if (sz != 0) in.read(out.data(), sz);
    return in.good() || in.eof();
}
bool FileManager::removeBlock(const std::string& hash)
{
    if (hash.empty()) return false;
    std::string path = (fs::path(blocksDir_)  / hash).string();
    std::error_code ec;
    return fs::remove(path, ec);
}
// 按块清单顺序拼接,重建完整文件
bool FileManager::rebuildFile(const std::vector<BlockInfo>& blocks, const std::string& targetPath)
{
    if (blocks.empty()) return false;

    std::string resolved = resolvePath(targetPath);   // 复用现有路径规范化
    std::error_code ec;
    fs::create_directories(fs::path(resolved).parent_path(), ec);

    std::string tmp = resolved + ".rebuild." + std::to_string(::getpid());

    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;

        for (const auto& b : blocks)                 // 调用方保证按 block_index 升序
        {
            std::vector<char> buf;
            if (!readBlock(b.hash, buf))             // 复用上一轮的 readBlock
            {
                out.close();
                fs::remove(tmp, ec);
                return false;
            }
            if (!buf.empty())
                out.write(buf.data(), static_cast<std::streamsize>(buf.size()));
            if (!out.good())
            {
                out.close();
                fs::remove(tmp, ec);
                return false;
            }
        }
        out.flush();
        if (!out.good())
        {
            out.close();
            fs::remove(tmp, ec);
            return false;
        }
    }

    fs::rename(tmp, resolved, ec);                   // 原子发布
    if (ec)
    {
        fs::remove(tmp, ec);
        return false;
    }
    return true;
}


bool FileManager::rebuildWholeFile(const std::vector<BlockInfo>& blocks,
                                   std::string& md5, const std::string& relPath)
{
    if (blocks.empty()) return false;
    std::string syncDir = (fs::path(baseDir_) / "sync").string();
    std::error_code ec;
    fs::create_directories(syncDir, ec);

    std::string tmp = (fs::path(syncDir) / (".tmp." + std::to_string(::getpid()))).string();

    MD5_CTX ctx; MD5_Init(&ctx);              // 流式算整文件 md5
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        for (const auto& b : blocks)
        {
            std::vector<char> buf;
            if (!readBlock(b.hash, buf)) { out.close(); fs::remove(tmp, ec); return false; }
            if (!buf.empty())
            {
                out.write(buf.data(), (std::streamsize)buf.size());
                MD5_Update(&ctx, buf.data(), buf.size());
            }
            if (!out.good()) { out.close(); fs::remove(tmp, ec); return false; }
        }
        out.flush();
        if (!out.good()) { out.close(); fs::remove(tmp, ec); return false; }
    }

    unsigned char md[MD5_DIGEST_LENGTH];
    MD5_Final(md, &ctx);
    char hex[33];
    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i)
        std::sprintf(hex + i * 2, "%02x", md[i]);
    hex[32] = '\0';

    md5 = hex;
    relPath = "sync/" + hex;                  // 整文件也按内容寻址 → 秒传去重可用
    fs::rename(tmp, (fs::path(syncDir) / hex).string(), ec);
    if (ec) { fs::remove(tmp, ec); return false; }
    return true;


}