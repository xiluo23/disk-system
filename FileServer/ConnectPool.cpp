#include "ConnectPool.h"
#include <chrono>

ConnectPool::ConnectPool(int initialSize)
    : maxSize_(initialSize > 0 ? initialSize : 1),
      created_(0),
      configOk_(false)
{
    Json::Value root;
    std::ifstream fs("config.json");
    if (!fs.is_open())
    {
        spdlog::error("ConnectPool: config.json not found");
        return;
    }
    Json::Reader reader;
    if (!reader.parse(fs, root))
    {
        spdlog::error("ConnectPool: failed to parse config.json");
        return;
    }

    host_     = root["host"].asString();
    user_     = root["user"].asString();
    password_ = root["password"].asString();
    database_ = root["database"].asString();
    configOk_ = true;

    for (int i = 0; i < initialSize; ++i)
        addConnection();

    std::lock_guard<std::mutex> lock(mtx_);
    spdlog::info("ConnectPool initialized: {}/{} connections",
                 created_, maxSize_);
}

ConnectPool::~ConnectPool()
{
    std::lock_guard<std::mutex> lock(mtx_);
    while (!connections_.empty())
    {
        MySQL* conn = connections_.front();
        connections_.pop();
        delete conn;
    }
    created_ = 0;
}

void ConnectPool::addConnection()
{
    if (!configOk_)
        return;

    MySQL* conn = new MySQL();
    if (!conn->connect(host_, user_, password_, database_))
    {
        spdlog::error("ConnectPool: failed to connect to database");
        delete conn;
        return;
    }

    connections_.push(conn);
    ++created_;
}

bool ConnectPool::alive() const
{
    std::lock_guard<std::mutex> lock(mtx_);
    return configOk_ && !connections_.empty();
}

MySQL* ConnectPool::getConnection()
{
    std::unique_lock<std::mutex> lock(mtx_);

    // 1. 有现成连接直接用
    if (!connections_.empty())
    {
        MySQL* c = connections_.front();
        connections_.pop();
        return c;
    }

    // 2. 没到硬上限 → 按需补建一个(避免池空即失败)
    if (configOk_ && created_ < maxSize_)
    {
        MySQL* conn = new MySQL();
        if (conn->connect(host_, user_, password_, database_))
        {
            ++created_;
            return conn;
        }
        delete conn;
        spdlog::error("ConnectPool: on-demand connect failed");
    }

    // 3. 都被借走(或 DB 故障)→ 等最多 5 秒有归还再用
    if (!cv_.wait_for(lock, std::chrono::seconds(5),
                      [this] { return !connections_.empty(); }))
    {
        spdlog::error("ConnectPool: acquire timeout");
        return nullptr;
    }

    MySQL* c = connections_.front();
    connections_.pop();
    return c;
}

void ConnectPool::InQue(MySQL* conn)
{
    if (!conn)
        return;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        connections_.push(conn);
    }
    cv_.notify_one();
}
