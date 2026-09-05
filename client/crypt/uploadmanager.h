#ifndef UPLOADMANAGER_H
#define UPLOADMANAGER_H

#include <QObject>
#include <QThreadPool>
#include <QString>
#include <atomic>
#include"ChunkTask.h"
#include"uploadtask.h"
#include<QMutex>
#include <QFile>
#include <QDebug>
#include<QFileInfo>

struct ChunkTask;

class UploadManager : public QObject
{
    Q_OBJECT

public:
    explicit UploadManager(QObject* parent=nullptr);
    /*
     * 开始上传文件
     */
    void uploadFile(
        const QString& filePath,
        qint64 uploadId,
        const QVector<int>& finishedChunks,
        const QString& token,
        const QString& clientId,
        const QByteArray& aesKey,
        const QString&parentPath,
        const QString&md5
        );
    void startTask(const ChunkTask&task);
signals:
    /*
     * 上传进度
     * value: 0-100
     */
    void progress(int value);
    /*
     * 上传完成
     */
    void uploadFinished();
    void uploadFailed();

private slots:
    void onChunkFinished(
        quint64 size
        );
    void onChunkFail(int index);
private:
    /*
     * 创建chunk任务
     */
    void createTasks();
private:
    QThreadPool _pool;
    QString _filePath;
    quint64 _fileSize;
    std::atomic<quint64>_uploadedBytes{0};
    QVector<int>_finishedChunks;
    qint64 _uploadId;
    QMutex _mutex;
    QString _fileName;
    QString _fileMd5;
    QString _token;
    QString _clientId;
    QByteArray _aesKey;
    QString _parentPath;

    std::unordered_map<int,int>retryCount;
    std::unordered_map<int,ChunkTask>_taskMap;
};

#endif