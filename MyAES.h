#ifndef MYAES_H
#define MYAES_H
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <string.h>
#include <vector>
#define KEY_SIZE 32   //AES 256byte
#define IV_SIZE 16
class MyAES{
public:
    MyAES();
    explicit MyAES(const unsigned char* key);

    ~MyAES();

    // 生成随机AES密钥
    bool generateKey();

    // 设置密钥
    void setKey(const unsigned char* key);

    // 获取密钥
    const unsigned char* getKey() const;

    // 获取密钥长度
    int getKeyLength() const;

    // AES-CBC加密
    bool encrypt(const unsigned char* plain,
                 int plainLen,
                 std::vector<unsigned char>& cipher);

    // AES-CBC解密
    bool decrypt(const unsigned char* cipher,
                 int cipherLen,
                 std::vector<unsigned char>& plain);

    bool generateIV();

    const unsigned char* getIV() const;

    void setIV(const unsigned char* iv);
private:
    unsigned char key_[KEY_SIZE];
    unsigned char iv_[IV_SIZE];
    AES_KEY encryptKey_;
    AES_KEY decryptKey_;
};

#endif