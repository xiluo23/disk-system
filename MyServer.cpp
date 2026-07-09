#include"MyServer.h"

MyServer::MyServer(muduo::net::EventLoop* loop, const muduo::net::InetAddress& listenAddr)
    : _server(loop, listenAddr, "MyServer"),
      _loop(loop)
{
    _server.setConnectionCallback(std::bind(&MyServer::onConnection, this, std::placeholders::_1));
    _server.setMessageCallback(std::bind(&MyServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    _server.setThreadNum(4); // 设置线程数为4
}

MyServer::~MyServer()
{

}

void MyServer::start()
{
    _server.start();
}

void MyServer::onConnection(const muduo::net::TcpConnectionPtr& conn){
    if (conn->connected())
    {
        std::cout << "New Connection: "
                  << conn->peerAddress().toIpPort()
                  << std::endl;
    }
    else
    {
        std::cout << "Connection Closed: "
                  << conn->peerAddress().toIpPort()
                  << std::endl;
    }
}

bool MyServer::check_sign(const Request&req){
    std::string signData;
    signData += std::to_string(req.type());
    signData += req.clientid();
    signData += req.serverid();
    signData += req.data();
    signData+=req.iv();
    std::vector<unsigned char>signature(signData.size());
    if(_rsaMap[req.clientid()]->rsa_verify((const unsigned char*)signData.data(),signData.size(),(const unsigned char*)req.sign().data(),req.sign().size())==-1){
        std::cout<<"rsa_verify error\n";
        return false;
    }
    return true;
}

void MyServer::handle_keyrevoke(const Request&req,Response&rsp){
    rsp.set_type(KEY_REVOKE);
    rsp.set_clientid(req.clientid());
    rsp.set_serverid(req.serverid());

    // 1. 验证签名
    if (!check_sign(req))
    {
        rsp.set_status(false);
        return;
    }

    // 2. 删除AES密钥
    _aesMap.erase(req.clientid());

    // 如果还保存RSA公钥
    _rsaMap.erase(req.clientid());

    rsp.set_status(true);
}

void MyServer::handle_keyagree(const Request&req,Response&rsp){
    cmdType type=KEY_AGREE;
    rsp.set_clientid(req.clientid());
    rsp.set_serverid(req.serverid());
    //解析公钥
    std::string pub_key=req.data();
    BIO* bio = BIO_new_mem_buf(pub_key.data(), pub_key.size());
    RSA* key = PEM_read_bio_RSA_PUBKEY(bio,
                                   nullptr,
                                   nullptr,
                                   nullptr);
    BIO_free(bio);
    if(key==nullptr){
        rsp.set_status(false);
        return ;
    }
    if(_rsaMap.find(req.clientid())==_rsaMap.end())
        _rsaMap[req.clientid()]=make_unique<MyRSA>(key);
    //验证签名  type+clientID+serverID+data
    if(check_sign(req)==false){
        rsp.set_status(false);
        std::cout<<"check_sign fail\n";
        return ;
    }
    //生成AES公钥
    if(_aesMap.find(req.clientid())==_aesMap.end())
        _aesMap[req.clientid()]=make_unique<MyAES>();
    if(!_aesMap[req.clientid()]->generateKey()){
        std::cout<<"aes generatekey fail\n";
        rsp.set_status(false);
        return ;
    }

    //RSA加密AES
    std::string aeskey((const char*)(_aesMap[req.clientid()]->getKey()),_aesMap[req.clientid()]->getKeyLength());
    std::vector<unsigned char>encrypted(_rsaMap[req.clientid()]->getKeySize());
    int len=_rsaMap[req.clientid()]->rsa_encrypt((const unsigned char*)aeskey.data(),aeskey.size(),encrypted.data());
    if(len==-1){
        std::cout<<"rsa encrypt fail\n";
        rsp.set_status(false);
    }
    rsp.set_status(true);
    rsp.set_data(encrypted.data(),len);
    rsp.set_seckeyid(1);
}
void MyServer::handle_keycheck(const Request&req,Response&rsp){
    rsp.set_type(KEY_CHECK);
    rsp.set_clientid(req.clientid());
    rsp.set_serverid(req.serverid());

    // 1. 查找AES密钥
    auto it = _aesMap.find(req.clientid());
    if (it == _aesMap.end())
    {
        std::cout<<"fail to find aes_key\n";
        rsp.set_status(false);
        return;
    }

    MyAES* aes = it->second.get();

    // 2. 验证签名
    if (!check_sign(req))
    {
        std::cout<<"fail to check_sign\n";
        rsp.set_status(false);
        return;
    }

    // 3. AES解密
    std::vector<unsigned char> plain;
    aes->setIV((const unsigned char*)req.iv().data());
    if (!aes->decrypt(
            reinterpret_cast<const unsigned char*>(req.data().data()),
            req.data().size(),
            plain))
    {
        std::cout<<"fail ase decrypt\n";
        rsp.set_status(false);
        return;
    }    
    if (plain.size() != sizeof(uint64_t))
    {
        std::cout<<"plain_size != 8B\n";
        rsp.set_status(false);
        return;
    }

    // 4. 取出nonce
    uint64_t nonce;

    memcpy(&nonce, plain.data(), sizeof(uint64_t));

    // 5. nonce+1
    nonce++;

    // 6. AES加密
    std::vector<unsigned char> cipher;

    if (!aes->encrypt(
            reinterpret_cast<unsigned char*>(&nonce),
            sizeof(uint64_t),
            cipher))
    {
        std::cout<<"aes encrypt fail\n";
        rsp.set_status(false);
        return;
    }

    // 7. 返回
    rsp.set_status(true);
    rsp.set_data(cipher.data(), cipher.size());
}

void MyServer::onMessage(const muduo::net::TcpConnectionPtr& conn, muduo::net::Buffer* buf, muduo::Timestamp time){
    std::string data=buf->retrieveAllAsString();
    Request req;
    Response rsp;
    if(!req.ParseFromString(data)){
        std::cout<<"ParseFromString fail\n";
        return ;
    }
    switch(req.type()){
        case KEY_AGREE:
            handle_keyagree(req,rsp);
            break;
        case KEY_CHECK:
            handle_keycheck(req,rsp);
            break;
        case KEY_REVOKE:
            handle_keyrevoke(req,rsp);
            break;
        default:
            break;
    }
    conn->send(rsp.SerializeAsString());
}