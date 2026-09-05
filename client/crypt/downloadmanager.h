#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H
#include<QObject>
#include<QThreadPool>
#include"downtask.h"
#include<QMutex>
#include"ChunkTask.h"
#include<QFile>
#include<QFileInfo>
const int CHUNK_SIZE=1024*1024;
class DownloadManager:public QObject
{
    Q_OBJECT
public:
    explicit DownloadManager(QObject* parent=nullptr);

    void startDownload(
        QString filename,QString path,uint64_t filesize,
        QString token,QByteArray iv,QByteArray aesKey,QString clientid,QString parentPath);

signals:
    // 一个chunk完成
    void chunkFinished(quint64 size,
        quint64 offset,
        QByteArray data);

    void chunkFail(int index);
    //整体进度
    void progress(
        uint64_t bytes);
    //下载完成
    void downloadFinished();
    void downloadFailed();
private slots:
    void onChunkFinished(quint64 size,quint64 offset,QByteArray data);
    void onChunkFail(int index);
private:

    std::unordered_map<int,int>_retryCount;
    std::unordered_map<int,ChunkTask>_taskMap;

    QMutex mutex_;

    QThreadPool pool_;

    std::atomic<int> finished_;

    uint64_t totalSize_;
    uint64_t downloadBytes_;
    QFile* file_;

};
#endif // DOWNLOADMANAGER_H
