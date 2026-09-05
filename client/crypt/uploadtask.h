#ifndef UPLOADTASK_H
#define UPLOADTASK_H
#include <QObject>
#include <QRunnable>
#include "ChunkTask.h"
#include"codec.h"
#include<QTcpSocket>
#include"File.pb.h"
#include <QFile>
#include <QTcpSocket>
#include <QDebug>
#include<QCryptographicHash>
#include"myaes.h"

class UploadTask :
                   public QObject,
                   public QRunnable
{

    Q_OBJECT
public:
    explicit UploadTask(
        const ChunkTask& task,
        QObject* parent=nullptr
        );
    void run() override;
signals:
    /*
     * 一个chunk上传完成
     */
    void chunkFinished(
        quint64 size
        );
    /*
     * chunk上传失败
     */
    void chunkFailed(
        int index
        );
private:
    /*
     * 读取chunk
     */
    QByteArray readChunk();
    /*
     * 上传chunk
     */
    bool uploadChunk(
        const QByteArray& data
        );
private:
    ChunkTask _task;
    QByteArray recvBuffer_;
};
#endif