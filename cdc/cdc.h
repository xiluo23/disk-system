#pragma once 
#include<string>
#include<stdint.h>
#include<vector>
struct CdcBlock{
    uint64_t offset;
    size_t size;
    std::string hash    ;
};
//内容定义分块

// BLAKE2s-256,用 OpenSSL EVP(两端已链 OpenSSL,零新依赖)
std::string blake2s_hex(const void* data, size_t len);

class CdcChunker{
public:
     // avg 平均块大小,min/max 兜底
    explicit CdcChunker(size_t avg = 128*1024, size_t min = 32*1024, size_t max = 512*1024);
    /// 整块切分,每切出一块就回调一次
    std::vector<CdcBlock> chunkAll(const char* data, size_t len) const;
private:
    size_t avg_, min_, max_;
    uint32_t cutMask_;    // 2^k-1,切块判定用
};
