#include "UploadManager.h"


const int CHUNK_SIZE =1024*1024;//1MB



UploadManager::UploadManager(QObject* parent)
    :
    QObject(parent)
{

    _pool.setMaxThreadCount(4);

}

void UploadManager::uploadFile(const QString& filePath,qint64 uploadId,
    const QVector<int>& finishedChunks,const QString& token,
    const QString& clientId,const QByteArray& aesKey,
    const QString&parentPath,const QString&md5)
{
    QFile file(filePath);
    if(!file.open(QIODevice::ReadOnly))
    {
        qDebug()
        <<"open file error";
        return;
    }
    _filePath=filePath;
    _fileMd5=md5;
    QFileInfo info(filePath);
    _fileName=
        info.fileName();
    _fileSize=
        file.size();
    _uploadId=
        uploadId;
    _token=
        token;
    _clientId=
        clientId;
    _aesKey=
        aesKey;
    _parentPath=parentPath;
    _finishedChunks=finishedChunks;
    _uploadedBytes=0;
    file.close();
    createTasks();
}

void UploadManager::createTasks()
{
    retryCount.clear();
    _taskMap.clear();
    QFile file(_filePath);
    if(!file.open(
        QIODevice::ReadOnly
            )){
        qDebug()<<"file open fail";
        return ;
    }
    int index=0;
    for(quint64 offset=0;offset<_fileSize;offset+=CHUNK_SIZE,index++)
    {
        quint64 size=
            qMin(
                (quint64)CHUNK_SIZE,
                _fileSize-offset
                );
        if(_finishedChunks.contains(index))
        {
            /*
              累加已经完成大小
              用于进度条
            */
            _uploadedBytes += size;
            index++;
            continue;
        }
        QMutexLocker<QMutex>lock(&_mutex);
        retryCount[index]=1;
        lock.unlock();

        ChunkTask task;
        task.offset=offset;
        task.parentPath=_parentPath;
        task.filePath=_filePath;
        task.uploadId=index;
        task.uploadId=_uploadId;
        task.fileName=_fileName;
        task.fileSize=_fileSize;
        task.filemd5=_fileMd5;
        task.token=_token;
        task.clientId=_clientId;
        task.aesKey=_aesKey;
        task.index=index;
        qDebug()<<"start task "<<index;
        startTask(task);
        _taskMap[index]=task;
    }
    int value=(_uploadedBytes*100)
                /
                _fileSize;
    emit progress(value);
}

void UploadManager::onChunkFinished(quint64 size)
{
    _uploadedBytes += size;
    int value =
        (_uploadedBytes * 100)
        /
        _fileSize;
    emit progress(value);
    if(_uploadedBytes==_fileSize){
        emit uploadFinished();
    }
}

void UploadManager::onChunkFail(int index){
    if(retryCount[index] < 3)
    {
        QMutexLocker<QMutex>lock(&_mutex);
        retryCount[index]++;
        startTask(_taskMap[index]);
        lock.unlock();
    }
    else
    {
        emit uploadFailed();

    }
}


void UploadManager::startTask(const ChunkTask& task)
{

    UploadTask* uploadTask =
        new UploadTask(task);


    connect(
        uploadTask,
        &UploadTask::chunkFinished,
        this,
        &UploadManager::onChunkFinished
        );


    connect(
        uploadTask,
        &UploadTask::chunkFailed,
        this,
        &UploadManager::onChunkFail
        );
    uploadTask->setAutoDelete(true);
    qDebug()<<"pool start task";
    _pool.start(uploadTask);
}



