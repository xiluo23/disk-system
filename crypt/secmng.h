#ifndef SECMNG_H
#define SECMNG_H
#include <QTcpSocket>
#include"Request.pb.h"
#include"Response.pb.h"
#include"myaes.h"
#include"myrsa.h"
#include<QRandomGenerator>
#include<QTimer>
class Secmng:public QObject
{
    Q_OBJECT
public:
    explicit Secmng(QObject*parent);
    ~Secmng();
    void generate_sign(const Request&req,std::vector<unsigned char>&signature,unsigned int&signature_len);

    void handle_keycheck(const Response&);
    void handle_keyagree(const Response&);
    void handle_keyrevoke(const Response&);
    void on_check();
    void on_revoke();
    void on_agree();

    bool encrypt(const QByteArray& plain,
                 QByteArray& cipher);

    bool decrypt(const QByteArray& cipher,
                 QByteArray& plain);

    void set_clientid(QString clientid);
    void setToken(QString token);
    std::string getIV();
signals:
    void agreeSuccess();
private slots:
    void onReadyRead();
    void checkKeyExpire();
private:
    QTcpSocket* socket_;
    MyRSA*rsa_;
    MyAES*aes_;
    uint64_t nonce_;
    uint32_t seckeyid_;
    QTimer*timer_;
    time_t expireTime_;
    QString clientid_;
    QString token_;
    void sendRequest(const Request&req);
};

#endif // SECMNG_H
