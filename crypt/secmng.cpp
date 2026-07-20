#include "secmng.h"

Secmng::Secmng(QObject*parent):QObject(parent),nonce_(1),seckeyid_(1) {
    rsa_=new MyRSA;
    rsa_->generate_key(2048);//256byte
    aes_=new MyAES;
    socket_=new QTcpSocket(this);
    socket_->connectToHost("192.168.234.129",8001);
    timer_=new QTimer(this);
    connect(timer_,
            &QTimer::timeout,
            this,
            &Secmng::checkKeyExpire);
    timer_->start(60 * 1000);


    connect(socket_,&QTcpSocket::connected,this,&Secmng::on_agree);
    connect(socket_,
            &QTcpSocket::disconnected,
            this,
            &Secmng::on_revoke
            );
    connect(socket_,
            &QTcpSocket::readyRead,
            this,
            &Secmng::onReadyRead
            );
}
void Secmng::set_clientid(QString clientid){
    clientid_=clientid;
}
void Secmng::setToken(QString token){
    token_=token;
}
void Secmng::checkKeyExpire()
{
    time_t now = time(nullptr);

    if(expireTime_ - now < 300)
    {
        qDebug() << "AES key will expire.";

        on_agree();      // 重新协商
    }
}
Secmng::~Secmng(){
    delete rsa_;
    delete aes_;
}


void Secmng::generate_sign(const Request&req,std::vector<unsigned char>&signature,unsigned int&signature_len){
    std::string signData;
    signData+=req.seckeyid();
    signData += std::to_string(req.type());
    signData += req.clientid();
    signData += req.serverid();
    signData += req.data();
    signData+=req.iv();
    signature.resize(rsa_->getKeySize());
    if(rsa_->rsa_sign((const unsigned char*)signData.data(),signData.size(),signature.data(),&signature_len)==-1){
        qDebug()<<"rsa_sign faile";
        return ;
    }
}
void Secmng::handle_keyagree(const Response&rsp){
    // RSA 解密 AES Key
    std::vector<unsigned char>aes_key(aes_->getKeyLength());

    int len = rsa_->rsa_decrypt(
        reinterpret_cast<const unsigned char*>(rsp.data().data()),
        rsp.data().size(),
        aes_key.data());

    if (len != aes_->getKeyLength())
    {
        qDebug() << "Invalid AES key";
        return;
    }

    // 保存 AES Key
    aes_->setKey(aes_key.data());

    qDebug() << "AES key received.";
}
void Secmng::handle_keycheck(const Response&rsp){
    std::vector<unsigned char> plain;

    if (!aes_->decrypt(
            (unsigned char*)rsp.data().data(),
            rsp.data().size(),
            plain))
    {
        qDebug() << "AES decrypt failed";
        return;
    }
    if (plain.size() != sizeof(uint64_t))
    {
        qDebug() << "Invalid nonce";
        return;
    }
    uint64_t value;
    memcpy(&value, plain.data(), sizeof(uint64_t));

    if (value == nonce_ + 1)
    {
        qDebug() << "AES key check success";
        emit agreeSuccess();
    }
    else
    {
        qDebug() << "AES key check failed";
    }
}
void Secmng::onReadyRead(){
    QByteArray buffer = socket_->readAll();

    Response rsp;
    if (!rsp.ParseFromArray(buffer.data(), buffer.size()))
    {
        qDebug() << "Parse response failed";
        return;
    }

    if (!rsp.status())
    {
        qDebug() << "Key agree failed";
        return;
    }
    switch(rsp.type()){
    case KEY_AGREE:
        handle_keyagree(rsp);
        on_check();
        break;
    case KEY_CHECK:
        handle_keycheck(rsp);
        break;
    case KEY_REVOKE:
        handle_keyrevoke(rsp);
        break;
    default:
        break;
    }

}
void Secmng::handle_keyrevoke(const Response&rsp){
    if (!rsp.status())
    {
        qDebug() << "KEY_REVOKE failed";
        return;
    }
    qDebug() << "AES Key revoked.";
}




void Secmng::on_check()
{
    nonce_ = QRandomGenerator::global()->generate64();
    std::vector<unsigned char>cipher;
    aes_->generateIV();
    aes_->encrypt(reinterpret_cast<unsigned char*>(&nonce_),sizeof(nonce_),cipher);

    Request req;
    req.set_iv(aes_->getIV(),IV_SIZE);
    req.set_type(KEY_CHECK);
    req.set_clientid(clientid_.toStdString());
    req.set_serverid("1");
    req.set_data(cipher.data(),cipher.size());
    req.set_seckeyid(std::to_string(seckeyid_));
    unsigned int signature_len=0;
    std::vector<unsigned char>signature;
    generate_sign(req,signature,signature_len);
    req.set_sign(signature.data(),signature_len);

    sendRequest(req);
    qDebug()<<"KEY_CHECK send";
}


void Secmng::on_revoke()
{
    Request req;
    req.set_type(KEY_REVOKE);
    req.set_clientid(clientid_.toStdString());
    req.set_serverid("1");
    req.set_data("");
    req.set_seckeyid(std::to_string(seckeyid_));
    req.set_iv("");

    unsigned int signLen;
    std::vector<unsigned char> sign;
    generate_sign(req, sign, signLen);

    req.set_sign(sign.data(), signLen);

    sendRequest(req);

    qDebug()<<"revoke send";
}


void Secmng::on_agree()
{
    qDebug()<<"on_agree";
    Request req;
    req.set_type(KEY_AGREE);
    req.set_clientid(clientid_.toStdString());
    req.set_serverid("1");
    req.set_seckeyid(std::to_string(seckeyid_++));
    req.set_data(rsa_->get_pubkey());
    req.set_iv("");
    //生成签名
    unsigned int signature_len=0;
    std::vector<unsigned char>signature;
    generate_sign(req,signature,signature_len);
    req.set_sign(signature.data(),signature_len);

    time_t now=time(NULL);
    expireTime_=now+3600;

    sendRequest(req);
}
void Secmng::sendRequest(const Request&req){
    std::string packet=req.SerializeAsString();
    socket_->write(packet.data(),packet.size());
}
std::string Secmng::getIV(){
    return std::string(reinterpret_cast<const char*>(aes_->getIV()),IV_SIZE);
}

bool Secmng::encrypt(const QByteArray& plain,
                     QByteArray& cipher)
{
    if (plain.isEmpty())
        return false;

    std::vector<unsigned char> out;

    if (!aes_->encrypt(
            reinterpret_cast<const unsigned char*>(plain.constData()),
            plain.size(),
            out))
    {
        return false;
    }

    cipher = QByteArray(reinterpret_cast<const char*>(out.data()),
                        static_cast<int>(out.size()));

    return true;
}

bool Secmng::decrypt(const QByteArray& cipher,
                     QByteArray& plain)
{
    if (cipher.isEmpty())
        return false;

    std::vector<unsigned char> out;

    if (!aes_->decrypt(
            reinterpret_cast<const unsigned char*>(cipher.constData()),
            cipher.size(),
            out))
    {
        return false;
    }

    plain = QByteArray(reinterpret_cast<const char*>(out.data()),
                       static_cast<int>(out.size()));

    return true;
}