#ifndef CHUNKTASK_H
#define CHUNKTASK_H
#include<QString>

struct ChunkTask
{
    quint64 index;
    QString filePath;
    QString fileName;
    QString parentPath;
    qint64 uploadId;
    quint64 fileSize;
    quint64 offset;
    uint32_t size;
    QByteArray aesKey;
    QString token;
    QString clientId;
    QString filemd5;
    QString iv;
};


#endif // CHUNKTASK_H
