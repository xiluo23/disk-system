#ifndef MYRSA_H
#define MYRSA_H

#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include<fstream>
class MyRSA
{
public:
    MyRSA();
    MyRSA(RSA* pub, RSA* pri = nullptr);
    ~MyRSA();

    // 生成RSA密钥对
    int generate_key(int bits);

    // 公钥加密
    int rsa_encrypt(const unsigned char* plain,
                    int plainLen,
                    unsigned char* cipher);

    // 私钥解密
    int rsa_decrypt(const unsigned char* cipher,
                    int cipherLen,
                    unsigned char* plain);

    // 私钥签名
    int rsa_sign(const unsigned char* data,
                 int dataLen,
                 unsigned char* signature,
                 unsigned int* signatureLen);

    // 公钥验签
    int rsa_verify(const unsigned char* data,
                   int dataLen,
                   const unsigned char* signature,
                   unsigned int signatureLen);

    // 获取RSA模长(例如2048位返回256)
    int getKeySize() const;

    // 获取PEM格式公钥
    std::string get_pubkey();

private:
    RSA* _pubkey = nullptr;
    RSA* _prikey = nullptr;
};

#endif