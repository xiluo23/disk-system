#ifndef SECKEYSHM_H
#define SECKEYSHM_H

#include "BaseShm.h"
#include <cstring>
#include <ctime>

constexpr int MAX_SECKEY_NUM = 1024;
constexpr int CLIENT_ID_LEN = 32;
constexpr int AES_KEY_LEN = 32;
constexpr int IV_LEN = 16;

struct SecKeyInfo
{
    bool used;

    char clientId[CLIENT_ID_LEN];

    uint serverId;
    uint secKeyId;

    unsigned char secKey[AES_KEY_LEN];

    time_t createTime;
    time_t expireTime;
};

class SecKeyShm : public BaseShm
{
public:
    explicit SecKeyShm(key_t key);

    bool init();

    bool insert(const SecKeyInfo& info);

    bool remove(const char* clientId);

    SecKeyInfo* find(const char* clientId);

    bool update(const SecKeyInfo& info);

private:
    SecKeyInfo* shmData_;
};

#endif