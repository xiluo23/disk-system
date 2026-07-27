#include "downtask.h"

DownloadTask::DownloadTask(ChunkTask task,QObject* parent):
    QObject(parent),task_(task) {
}

void DownloadTask::run()
{
    QByteArray data;
    if(downloadChunk(data)){
        emit chunkFinished(data.size(),task_.offset,data);
    }
    else{
        emit chunkFail(task_.index);
    }
}
QString calcMd5(std::vector<unsigned char>&plain){
    unsigned char md[MD5_DIGEST_LENGTH];
    MD5((const unsigned char*)plain.data(),plain.size(),md);
    char buf[33];
    for(int i = 0; i < MD5_DIGEST_LENGTH; ++i)
    {
        sprintf(buf + i * 2, "%02x", md[i]);
    }

    buf[32] = '\0';
    return QString(buf);
}
bool DownloadTask::downloadChunk(QByteArray&data){
    QTcpSocket socket;
    socket.connectToHost(
        "192.168.234.129",
        8003
        );
    qDebug()<<"build connection";
    if(!socket.waitForConnected(3000))
    {
        qDebug()<<"connect server fail";
        return false;
    }
    FileRequest req;
    req.set_type(DOWNLOAD_FILE);
    req.set_filename(task_.fileName.toStdString());
    req.set_offset(task_.offset);
    req.set_clientid(task_.clientId.toStdString());
    req.set_token(task_.token.toStdString());
    req.set_iv(task_.iv.toStdString());
    req.set_path(task_.parentPath.toStdString());
    std::string packet=Codec::encode(req);
    qint64 len =
        socket.write(
            packet.data(),
            packet.size()
            );
    if(len <= 0)
    {
        qDebug()<<"send fail";
        return false;
    }
    qDebug()<<"send req";
    if(!socket.waitForBytesWritten(3000))
    {
        qDebug()<<"write timeout";
        return false;
    }

    FileResponse rsp;
    recvBuffer_.clear();
    while (true)
    {
        if(!socket.waitForReadyRead(3000))
        {
            qDebug()<<"server response timeout";
            return false;
        }
        recvBuffer_.append(socket.readAll());
        qDebug()<<"recvBuffer size "<<recvBuffer_.size();
        if (recvBuffer_.size() < 4)
            continue;

        uint32_t len;
        memcpy(&len, recvBuffer_.constData(), 4);
        len = ntohl(len);

        if (recvBuffer_.size() < 4 + len)
            continue;

        QByteArray body = recvBuffer_.mid(4, len);

        recvBuffer_.remove(0, 4 + len);

        if (!rsp.ParseFromArray(body.data(), body.size()))
            continue;
        socket.disconnectFromHost();
        MyAES aes;
        aes.setIV((const unsigned char*)task_.iv.toStdString().data());
        aes.setKey((const unsigned char*)task_.aesKey.data());
        std::vector<unsigned char> plain;
        if(!aes.decrypt((const unsigned char*)rsp.data().data(),rsp.data().size(),plain)){
            qDebug()<<"aes decrypt fail";
            return false;
        }

        if(rsp.md5()!=calcMd5(plain)){
            qDebug()<<"md5 fail";
            return false;
        }
        data.append(
            reinterpret_cast<const char*>(plain.data()),
            plain.size());
        qDebug()<<"downtask recv data "<<data.size();
        return rsp.status();
    }
    return false;
}