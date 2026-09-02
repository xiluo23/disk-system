#pragma once 
#include"ConnectPool.h"


class Guard {
public:
    Guard(ConnectPool* pool) : _pool(pool), _conn(nullptr) {
        _conn = _pool->getConnection();
        if (!_conn) {
            spdlog::error("Failed to acquire a database connection from the pool");
        }
    }

    ~Guard() {
        if (_conn) {
            _pool->InQue(_conn);
        }
    }

    MySQL* getConnection() const {
        return _conn;
    }
private:
    ConnectPool* _pool;
    MySQL* _conn;

};