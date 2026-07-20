#ifndef BASESHM_H
#define BASESHM_H
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string>

class BaseShm{
public:
    BaseShm(key_t key, size_t size);
    virtual ~BaseShm();

    // 创建共享内存
    bool create();

    // 打开已有共享内存
    bool open();

    // 映射
    void* attach();

    // 解除映射
    bool detach();

    // 删除共享内存
    bool destroy();

protected:
    int shmid_;
    key_t key_;
    size_t size_;
    void* addr_;


};
#endif 