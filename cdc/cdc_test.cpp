// #include "cdc.h"
// #include <cassert>
// #include <cstdio>
// #include <cstring>
// #include <random>
// #include <vector>
// #include <algorithm>
// #include<set>
// #include<iostream>


// static std::vector<char> makeData(size_t n, unsigned seed) {
//     std::vector<char> d(n);
//     std::mt19937 rng(seed);
//     std::uniform_int_distribution<int> dist(0, 255);
//     for (auto& c : d) c = (char)dist(rng);
//     return d;
// }

// int main() {
//     const size_t FILE_SIZE = 8 * 1024 * 1024;   // 8MB 测试数据
//     CdcChunker chunker(128 * 1024, 32 * 1024, 512 * 1024);

//     // ---------- 性质1:确定性 ----------
//     auto data = makeData(FILE_SIZE, 42);
//     auto b1 = chunker.chunkAll(data.data(), data.size());
//     auto b2 = chunker.chunkAll(data.data(), data.size());
//     assert(b1.size() == b2.size());
//     for (size_t i = 0; i < b1.size(); ++i) {
//         assert(b1[i].offset == b2[i].offset);
//         assert(b1[i].size == b2[i].size);
//         assert(b1[i].hash == b2[i].hash);
//     }
//     printf("[PASS] determinism, %zu blocks\n", b1.size());

//     // ---------- 性质2:可无损还原(无缝隙、无重叠、内容一致) ----------
//     size_t pos = 0;
//     for (auto& b : b1) {
//         assert(b.offset == pos);                       // 块首尾相接
//         assert(memcmp(data.data() + pos, data.data() + b.offset, b.size) == 0);
//         pos += b.size;
//     }
//     assert(pos == data.size());                        // 恰好覆盖整个文件
//     printf("[PASS] lossless round-trip\n");

//     // ---------- 性质3:块大小约束 ----------
//     for (size_t i = 0; i < b1.size(); ++i) {
//         assert(b1[i].size <= 512 * 1024);              // 都不超最大块
//         if (i + 1 < b1.size())                         // 除最后一块外都不小于最小块
//             assert(b1[i].size >= 32 * 1024);
//     }
//     printf("[PASS] min/max chunk size\n");

//     // ---------- 性质4(核心):改 1 字节,只有极少数块变化 ----------
//     auto data2 = data;                                 // 改正中间 1 字节
//     data2[data2.size() / 2] ^= 0x01;
//     auto b3 = chunker.chunkAll(data2.data(), data2.size());
//     std::vector<int> diff;
//     {
//         std::set<std::string> h1;
//         for (auto& b : b1) h1.insert(b.hash);
//         for (auto& b : b3)
//             if (!h1.count(b.hash)) diff.push_back(1);
//     }
//     size_t changed = diff.size();
//     printf("[INFO] change 1 byte -> %zu changed blocks (of %zu)\n",
//            changed, b3.size());
//     assert(changed <= 8);                              // 应是个很小的常数
//     printf("[PASS] small edit -> small delta\n");

//     // ---------- 性质5:中间插入 1 字节(增量同步最关键) ----------
//     std::vector<char> data3(data.begin(), data.end());
//     data3.insert(data3.begin() + data3.size() / 2, (char)0xAB);
//     auto b4 = chunker.chunkAll(data3.data(), data3.size());
//     std::set<std::string> h1;
//     for (auto& b : b1) h1.insert(b.hash);
//     size_t changed2 = 0;
//     for (auto& b : b4)
//         if (!h1.count(b.hash)) ++changed2;
//     printf("[INFO] insert 1 byte -> %zu changed blocks\n", changed2);
//     assert(changed2 <= 8);
//     printf("[PASS] 1-byte insert -> only local blocks change\n");

//     printf("\nALL TESTS PASSED\n");

//     for(auto&x:b1){
//         std::cout<<x.size<<std::endl;;
//     }
//     return 0;
// }se