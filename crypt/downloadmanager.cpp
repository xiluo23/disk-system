#include "downloadmanager.h"

DownloadManager::DownloadManager(QObject* parent)
    :QObject(parent)
{
    pool_.setMaxThreadCount(4);
}

void DownloadManager::startDownload(QString filename,QString path,
    uint64_t filesize,QString token,QByteArray iv,QByteArray aesKey,
    QString clientid,QString parentPath)
{
    finished_=0;
    downloadBytes_=0;
    totalSize_=filesize;
    file_=new QFile(path,this);
    uint64_t offset=0;
    if(QFile::exists(path)){
        QFileInfo info(path);
        offset=info.size();
    }
    if(!file_->open(QIODevice::ReadWrite)){
        emit downloadFailed();
        return ;
    }
    quint32 value=offset/filesize*100;
    emit DownloadManager::progress(value);
    uint32_t index=0;
    _taskMap.clear();
    _retryCount.clear();
    for(;offset<filesize;offset+=CHUNK_SIZE,index++)
    {
        ChunkTask task;
        task.offset=offset;
        task.size=
            std::min<uint64_t>(
                CHUNK_SIZE,
                filesize-offset);
        task.index=index;
        task.clientId=clientid;
        task.token=token;
        task.fileName=filename;
        task.iv=iv;
        task.fileSize=filesize;
        task.aesKey=aesKey;
        task.parentPath=parentPath;
        _taskMap[index]=task;
        _retryCount[index]=1;
        DownloadTask* t=new DownloadTask(task);
        connect(t,
                &DownloadTask::chunkFinished,
                this,
                &DownloadManager::onChunkFinished);


        connect(t,
                &DownloadTask::chunkFail,
                this,
                &DownloadManager::onChunkFail);

        t->setAutoDelete(true);
        pool_.start(t);
    }
}

void DownloadManager::onChunkFinished(
    quint64 size,
    quint64 offset,
    QByteArray data)
{
    QMutexLocker locker(&mutex_);
    if(!file_->seek(offset))
    {
        emit downloadFailed();
        return;
    }


    file_->write(data);
    downloadBytes_ += size;
    int value =
        (downloadBytes_*100)
        /
        totalSize_;
    emit progress(value);
    if(downloadBytes_ == totalSize_)
    {
        finished_=1;
        file_->close();
        emit downloadFinished();
    }
}

void DownloadManager::onChunkFail(int index){
    if(_retryCount[index] < 3)
    {
        _retryCount[index]++;
        ChunkTask task=_taskMap[index];
        DownloadTask* t=new DownloadTask(task);
        connect(t,
                &DownloadTask::chunkFinished,
                this,
                &DownloadManager::onChunkFinished);


        connect(t,
                &DownloadTask::chunkFail,
                this,
                &DownloadManager::onChunkFail);

        t->setAutoDelete(true);
        pool_.start(t);
    }
    else
    {
        emit downloadFailed();
    }
}





