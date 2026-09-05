#include "uploadtask.h"
#include<openssl/md5.h>
std::string calcMd5(const QByteArray&plain){
    unsigned char md[MD5_DIGEST_LENGTH];
    MD5((const unsigned char*)plain.data(),plain.size(),md);
    char buf[33];
    printf("MD5:");
    for(int i = 0; i < MD5_DIGEST_LENGTH; ++i)
    {
        sprintf(buf + i * 2, "%02x", md[i]);
        printf("%02x",buf[i]);
    }
    printf("\n");
    buf[32] = '\0';
    return std::string(buf);
}
UploadTask::UploadTask(
    const ChunkTask& task,
    QObject* parent
    )
    :
    QObject(parent),
    _task(task)
{
    /*
       QRunnable执行完成自动释放
    */
    setAutoDelete(true);

}


void UploadTask::run()
{
    try{
        QByteArray data =readChunk();
        // qDebug()<<"read data "<<data.size();
        if(data.isEmpty())
        {
            emit chunkFailed(
                _task.index
                );
            return;
        }
        bool ret =uploadChunk(data);
        if(ret)
        {
            emit chunkFinished(
                data.size()
                );
        }
        else
        {
            emit chunkFailed(
                _task.index
                );
        }
    }
    catch(const std::exception&ex){
        qWarning() << "upload worker exception:" << ex.what();
        emit chunkFailed(_task.index);
    }
    catch(...){
        qWarning() << "upload worker unknown exception";
        emit chunkFailed(_task.index);
    }
}

QByteArray UploadTask::readChunk()
{
    QFile file(
        _task.filePath
        );
    if(!file.open(
            QIODevice::ReadOnly))
    {
        qDebug()<<"file open fail";
        return {};
    }
    /*
       跳到chunk位置
    */
    if(!file.seek(
            _task.offset))
    {
        qDebug()<<"seek fail ";
        return {};
    }
    QByteArray data =
        file.read(
            _task.chunkSize
            );
    file.close();
    // qDebug()<<"file size "<<data.size();
    return data;
}

bool UploadTask::uploadChunk(const QByteArray& data)
{
    /*
        1. 连接服务器
    */
    QTcpSocket socket;
    socket.connectToHost(
        "192.168.234.129",
        8003
        );
    // qDebug()<<"build connection";
    if(!socket.waitForConnected(3000))
    {
        qDebug()<<"connect server fail";
        return false;
    }
    /*
        2. AES加密chunk

        data:
        原始文件数据

        cipher:
        加密后的数据
    */
    std::string key =
        _task.aesKey.toStdString();

    MyAES aes(
        (const unsigned char*)key.data()
        );
    std::vector<unsigned char> cipher;
    if(!aes.encrypt(
            (const unsigned char*)data.constData(),
            data.size(),
            cipher))
    {
        qDebug()<<"encrypt fail";
        return false;
    }
    QByteArray iv(aes.getIV());
    QByteArray cipherData(
        (char*)cipher.data(),
        cipher.size()
        );

    /*
        4. 构造protobuf请求
    */
    FileRequest req;
    /*
        命令类型

        注意：
        这里应该是分片上传
    */
    req.set_type(
        UPLOAD_FILE
        );
    /*
        用户信息
    */
    req.set_token(
        _task.token.toStdString()
        );
    req.set_clientid(
        _task.clientId.toStdString()
        );
    req.set_md5(_task.filemd5.toStdString());
    /*
        上传任务ID

        对应upload_task表
    */
    req.set_upload_id(
        _task.uploadId
        );
    req.set_chunk_index(_task.index);
    /*
        文件信息
    */
    req.set_filename(
        _task.fileName.toStdString()
        );
    req.set_filesize(
        _task.fileSize
        );
    req.set_path(
        _task.parentPath.toStdString()
        );
    /*
        chunk信息
    */
    req.set_offset(
        _task.offset
        );
    req.set_chunk_size(
        data.size()
        );

    /*
        AES密文
    */
    req.set_data(
        cipherData.constData(),
        cipherData.size()
        );
    /*
        IV

        服务端解密需要
    */
    req.set_iv(
        iv.constData(),iv.size()
        );
    /*
        chunk MD5
    */
    std::string chunkMd5 =calcMd5(data);
    req.set_chunk_md5(chunkMd5);
    // qDebug()<<chunkMd5;
    /*
        5. protobuf编码
    */
    std::string packet =
        Codec::encode(req);
    /*
        6. TCP发送
    */
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
    // qDebug()<<"send chunk";
    if(!socket.waitForBytesWritten(3000))
    {
        qDebug()<<"write timeout";
        return false;
    }
    /*
        7. 等待服务器响应

        服务端返回:
        FileResponse
    */
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
        // qDebug()<<"rsp:"<<rsp.status();
        return rsp.status();
    }
    return false;
}

