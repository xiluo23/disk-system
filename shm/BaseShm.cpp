#include"BaseShm.h"
#include <unistd.h>
#include <cstring>

BaseShm::BaseShm(key_t key,size_t size)
{
    key_ = key;
    size_ = size;
    shmid_ = -1;
    addr_ = nullptr;
}

BaseShm::~BaseShm()
{
    detach();
}

bool BaseShm::create()
{
    shmid_ = shmget(key_, size_, IPC_CREAT | 0666);

    return shmid_ != -1;
}

bool BaseShm::open()
{
    shmid_ = shmget(key_, size_, 0666);

    return shmid_ != -1;
}

void* BaseShm::attach()
{
    if(shmid_ == -1)
        return nullptr;

    addr_ = shmat(shmid_, nullptr, 0);

    if(addr_ == (void*)-1)
    {
        addr_ = nullptr;
    }

    return addr_;
}

bool BaseShm::detach()
{
    if(addr_)
    {
        shmdt(addr_);
        addr_ = nullptr;
    }

    return true;
}

bool BaseShm::destroy()
{
    if(shmid_ == -1)
        return false;

    return shmctl(shmid_, IPC_RMID, nullptr) != -1;
}