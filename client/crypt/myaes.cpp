#include"MyAES.h"
#include<QDebug>
MyAES::MyAES()
{
    memset(key_, 0, KEY_SIZE);
    memset(iv_, 0, IV_SIZE);
}

MyAES::MyAES(const unsigned char* key)
{
    setKey(key);
    memset(iv_, 0, IV_SIZE);
}

MyAES::~MyAES()
{

}

bool MyAES::generateKey()
{
    if (RAND_bytes(key_, KEY_SIZE) != 1)
        return false;

    AES_set_encrypt_key(key_, KEY_SIZE * 8, &encryptKey_);
    AES_set_decrypt_key(key_, KEY_SIZE * 8, &decryptKey_);

    return true;
}

void MyAES::setKey(const unsigned char* key)
{
    memcpy(key_, key, KEY_SIZE);

    AES_set_encrypt_key(key_, KEY_SIZE * 8, &encryptKey_);
    AES_set_decrypt_key(key_, KEY_SIZE * 8, &decryptKey_);
}

const unsigned char* MyAES::getKey() const
{
    return key_;
}

int MyAES::getKeyLength() const
{
    return KEY_SIZE;
}

bool MyAES::generateIV()
{
    return RAND_bytes(iv_, IV_SIZE) == 1;
}

const unsigned char* MyAES::getIV() const
{
    return iv_;
}

void MyAES::setIV(const unsigned char* iv)
{
    memcpy(iv_, iv, IV_SIZE);
}

bool MyAES::encrypt(const unsigned char* plain,
                    int plainLen,
                    std::vector<unsigned char>& cipher)
{
    int pad = AES_BLOCK_SIZE - (plainLen % AES_BLOCK_SIZE);

    if (pad == 0)
        pad = AES_BLOCK_SIZE;

    int totalLen = plainLen + pad;

    cipher.resize(totalLen);

    memcpy(cipher.data(), plain, plainLen);

    // PKCS7 Padding
    memset(cipher.data() + plainLen, pad, pad);
    generateIV();
    unsigned char iv[AES_BLOCK_SIZE];
    memcpy(iv, iv_, AES_BLOCK_SIZE);

    AES_cbc_encrypt(cipher.data(),
                    cipher.data(),
                    totalLen,
                    &encryptKey_,
                    iv,
                    AES_ENCRYPT);

    return true;
}


bool MyAES::decrypt(const unsigned char* cipher,
                    int cipherLen,
                    std::vector<unsigned char>& plain)
{
    if (cipherLen % AES_BLOCK_SIZE)
        return false;

    plain.resize(cipherLen);

    unsigned char iv[AES_BLOCK_SIZE];
    memcpy(iv, iv_, AES_BLOCK_SIZE);

    AES_cbc_encrypt(cipher,
                    plain.data(),
                    cipherLen,
                    &decryptKey_,
                    iv,
                    AES_DECRYPT);
    unsigned char pad = plain.back();

    if (pad == 0 || pad > AES_BLOCK_SIZE)
        return false;

    for (int i = 0; i < pad; ++i)
    {
        if (plain[cipherLen - 1 - i] != pad)
            return false;
    }

    plain.resize(cipherLen - pad);
    return true;
}