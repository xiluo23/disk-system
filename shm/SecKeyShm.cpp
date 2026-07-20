#include "SecKeyShm.h"

SecKeyShm::SecKeyShm(key_t key)
    : BaseShm(key, sizeof(SecKeyInfo) * MAX_SECKEY_NUM)
{
    shmData_ = nullptr;
}

bool SecKeyShm::init()
{
    if (!create())
    {
        if (!open())
            return false;
    }

    shmData_ = static_cast<SecKeyInfo*>(attach());

    if (shmData_ == nullptr)
        return false;

    return true;
}

bool SecKeyShm::insert(const SecKeyInfo& info)
{
    for (int i = 0; i < MAX_SECKEY_NUM; i++)
    {
        if (!shmData_[i].used)
        {
            shmData_[i] = info;
            shmData_[i].used = true;
            return true;
        }
    }

    return false;
}

bool SecKeyShm::remove(const char* clientId)
{
    for (int i = 0; i < MAX_SECKEY_NUM; i++)
    {
        if (shmData_[i].used &&
            strcmp(shmData_[i].clientId, clientId) == 0)
        {
            memset(&shmData_[i], 0, sizeof(SecKeyInfo));
            return true;
        }
    }
    return false;
}

SecKeyInfo* SecKeyShm::find(const char* clientId)
{
    for (int i = 0; i < MAX_SECKEY_NUM; i++)
    {
        if (shmData_[i].used &&
            strcmp(shmData_[i].clientId, clientId) == 0)
        {
            return &shmData_[i];
        }
    }
    return nullptr;
}

bool SecKeyShm::update(const SecKeyInfo& info)
{
    SecKeyInfo* p = find(info.clientId);

    if (p == nullptr)
        return false;

    *p = info;
    p->used = true;

    return true;
}