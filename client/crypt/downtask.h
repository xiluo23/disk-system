#ifndef DOWNTASK_H
#define DOWNTASK_H

#include <QObject>
#include<QRunnable>
#include"ChunkTask.h"
#include<QTcpSocket>
#include"File.pb.h"
#include"codec.h"
#include<openssl/md5.h>
#include"myaes.h"
class DownloadTask:public QObject,public QRunnable
{
Q_OBJECT
public:
    explicit DownloadTask(ChunkTask task,QObject* parent=nullptr);
    void run() override;
    bool downloadChunk(QByteArray&);

signals:
    void chunkFinished(
        quint64 size,
        quint64 offset,
        QByteArray data);


    void chunkFail(
        int index);

private:
    ChunkTask task_;
    QByteArray recvBuffer_;
};

#endif // DOWNTASK_H
