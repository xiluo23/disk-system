#include "cdc.h"
#include <openssl/evp.h>
#include <cstdio>
#include <cstring>
#include <algorithm>

// ---------- BLAKE2s-256 ----------
std::string blake2s_hex(const void* data, size_t len) {
    unsigned char out[32];
    unsigned int outlen = 0;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();//动态分配一个摘要上下文所需的内存空间
    EVP_DigestInit_ex(ctx, EVP_blake2s256(), nullptr);
    EVP_DigestUpdate(ctx, data, len);
    EVP_DigestFinal_ex(ctx, out, &outlen);
    EVP_MD_CTX_free(ctx);

    char buf[65];
    for (unsigned i = 0; i < outlen; ++i)
        std::sprintf(buf + i*2, "%02x", out[i]);
    buf[outlen*2] = '\0';
    return buf;
}

// ---------- CDC ----------
// 滚动窗口 + 基数(base)与窗口长度是常量,只需全局一份
namespace {
constexpr size_t kWindow = 48;   // 滑动窗口字节数
constexpr uint32_t kBase  = 257; // 基数(奇素数,mod 2^32 环上可逆)

uint32_t rollingPower() {        // kBase^kWindow mod 2^32,uint 溢出即取模
    uint32_t p = 1;
    for (size_t i = 0; i < kWindow; ++i) p *= kBase;
    return p;
}
}

CdcChunker::CdcChunker(size_t avg, size_t min, size_t max)
    : avg_(avg), min_(min), max_(max) {
    if (max_ < min_) max_ = min_;
    // cutBits = ceil(log2(avg)),使 (h & mask)==0 的概率 ≈ 1/avg
    size_t bits = 0;
    for (size_t v = avg_ - 1; v; v >>= 1) ++bits;
    cutMask_ = (1u << bits) - 1;
}

std::vector<CdcBlock> CdcChunker::chunkAll(const char* data, size_t len) const {
    std::vector<CdcBlock> out;
    if (!data || len == 0) return out;

    const uint32_t pw = rollingPower();
    uint32_t h = 0;
    size_t start = 0;

    for (size_t i = 0; i < len; ++i) {
        // --- Rabin 滚动哈希:窗口 [i-kWindow+1, i] ---
        h = h * kBase + (uint8_t)data[i];
        if (i >= kWindow)
            h -= (uint8_t)data[i - kWindow] * pw;   // uint32 溢出=mod 2^32

        size_t chunkLen = (i + 1) - start;          // 候选块长度
        if (chunkLen < min_) continue;              // 不到最小块,不切

        bool cut;
        if (chunkLen >= max_)
            cut = true;                             // 到最大块,强制切
        else
            cut = ((h & cutMask_) == 0);            // 内容决定是否切

        if (cut) {
            out.push_back({ start, chunkLen, blake2s_hex(data + start, chunkLen) });
            start = i + 1;
        }
    }
    if (start < len) {                              // 尾部
        out.push_back({ start, len - start, blake2s_hex(data + start, len - start) });
    }
    return out;
}