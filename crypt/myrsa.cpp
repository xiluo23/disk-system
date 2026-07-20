#include"MyRSA.h"

MyRSA::MyRSA() {
}

MyRSA::~MyRSA() {
    if(_pubkey)
        RSA_free(_pubkey);
    if(_prikey)
        RSA_free(_prikey);
}


int MyRSA::generate_key(int bits)
{
    // 1. 判断密钥是否已经存在
    std::ifstream pub("public_key.pem");
    std::ifstream pri("private_key.pem");

    if (pub.good() && pri.good())
    {
        pub.close();
        pri.close();

        // 已存在，直接加载
        FILE* pub_fp = fopen("public_key.pem", "rb");
        FILE* pri_fp = fopen("private_key.pem", "rb");

        _pubkey = PEM_read_RSAPublicKey(pub_fp, nullptr, nullptr, nullptr);
        _prikey = PEM_read_RSAPrivateKey(pri_fp, nullptr, nullptr, nullptr);

        fclose(pub_fp);
        fclose(pri_fp);

        if (_pubkey && _prikey)
        {
            return 1;
        }

        // 文件损坏，继续重新生成
        if (_pubkey)
            RSA_free(_pubkey);
        if (_prikey)
            RSA_free(_prikey);

        _pubkey = nullptr;
        _prikey = nullptr;
    }

    // 2. 生成新的RSA密钥
    RSA* rsa = RSA_new();
    BIGNUM* e = BN_new();

    BN_set_word(e, RSA_F4);

    if (RSA_generate_key_ex(rsa, bits, e, nullptr) != 1)
    {
        BN_free(e);
        RSA_free(rsa);
        return -1;
    }

    _pubkey = RSAPublicKey_dup(rsa);
    _prikey = RSAPrivateKey_dup(rsa);

    FILE* pub_fp = fopen("public_key.pem", "wb");
    FILE* pri_fp = fopen("private_key.pem", "wb");

    PEM_write_RSAPublicKey(pub_fp, _pubkey);
    PEM_write_RSAPrivateKey(pri_fp, _prikey, nullptr, nullptr, 0, nullptr, nullptr);

    fclose(pub_fp);
    fclose(pri_fp);

    BN_free(e);
    RSA_free(rsa);


    return 1;
}

int MyRSA::rsa_encrypt(const unsigned char* data, int data_len, unsigned char* encrypted) {
    if(RSA_size(_pubkey) == 0) {
        perror("Public key is not initialized");
        return -1;
    }
    int ret = RSA_public_encrypt(data_len, data, encrypted, _pubkey, RSA_PKCS1_OAEP_PADDING);
    if (ret == -1) {
        ERR_print_errors_fp(stderr);
        return -1;
    }
    return ret;
}

int MyRSA::rsa_decrypt(const unsigned char* encrypted, int encrypted_len, unsigned char* decrypted) {
    if(RSA_size(_prikey) == 0) {
        perror("Private key is not initialized");
        return -1;
    }
    int ret = RSA_private_decrypt(encrypted_len, encrypted, decrypted, _prikey, RSA_PKCS1_OAEP_PADDING);
    if (ret == -1) {
        ERR_print_errors_fp(stderr);
        return -1;
    }
    return ret;
}

int MyRSA::rsa_sign(const unsigned char* data, int data_len, unsigned char* signature, unsigned int* signature_len) {
    if(RSA_size(_prikey) == 0) {
        perror("Private key is not initialized");
        return -1;
    }
    unsigned char digest[SHA256_DIGEST_LENGTH];
    //得到SHA256摘要
    SHA256(data,data_len,digest);
    int ret = RSA_sign(NID_sha256, digest, SHA256_DIGEST_LENGTH, signature, signature_len, _prikey);
    if (ret != 1) {
        ERR_print_errors_fp(stderr);
        return -1;
    }
    return 1;
}

int MyRSA::rsa_verify(const unsigned char* data, int data_len, const unsigned char* signature, unsigned int signature_len) {
    if(RSA_size(_pubkey) == 0) {
        perror("Public key is not initialized");
        return -1;
    }
    unsigned char digest[SHA256_DIGEST_LENGTH];
    //得到SHA256摘要
    SHA256(data,data_len,digest);
    int ret = RSA_verify(NID_sha256, digest, SHA256_DIGEST_LENGTH, signature, signature_len, _pubkey);
    if (ret != 1) {
        ERR_print_errors_fp(stderr);
        return -1;
    }
    return 1;
}

int MyRSA::getKeySize() const
{
    if (_prikey)
        return RSA_size(_prikey);
    if (_pubkey)
        return RSA_size(_pubkey);
    return 0;

}
std::string MyRSA::get_pubkey() const
{
    if (_pubkey == nullptr)
    {
        return "";
    }

    BIO* bio = BIO_new(BIO_s_mem());
    if (bio == nullptr)
    {
        return "";
    }

    // 写入 PEM 格式公钥
    if (!PEM_write_bio_RSA_PUBKEY(bio, _pubkey))
    {
        BIO_free(bio);
        return "";
    }

    BUF_MEM* mem = nullptr;
    BIO_get_mem_ptr(bio, &mem);

    std::string pubkey(mem->data, mem->length);

    BIO_free(bio);

    return pubkey;
}