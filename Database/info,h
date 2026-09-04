#ifndef INFO_H
#define INFO_H
#include<iostream>
#include<string>
#include <cstdint>
struct User
{
    int id = 0;
    std::string email;
    std::string password;
    std::string token;
};

struct FileInfo
{
    std::string name;
    std::string path;
    bool isDir = false;
    std::uint64_t size = 0;
};

struct BlockInfo
{
    int id = 0;
    std::string hash;
    std::uint64_t size = 0;
    std::uint64_t refCount = 0;
    std::string storagePath;
};

#endif