#pragma once
#include <QString>
#include <QVector>
#include <QHash>

// 清单里的一块:hash 用于去重/比对,offset/size 用于从本地文件切片上传
struct SyncBlock
{
    QString hash;
    qint64  offset = 0;
    qint64  size   = 0;
};

// 一个"已上传的本地文件"的同步状态,以 localPath 为键
struct SyncRecord
{
    QString localPath;                 // 本地绝对路径(键)
    QString cloudDir;                  // 云端目录:"/" 或 "/dir"
    QString cloudName;                 // 云端文件名
    qint64  mtime = 0;                 // 最近一次成功同步/登记时的 mtime(ms)
    qint64  size  = 0;
    int     version = -1;              // 服务端已接受的块版本;-1 = 普通上传后尚未建块版本
    QVector<SyncBlock> manifest;       // 已成功同步的块清单(普通上传后为空)
};

// 持久化到一个 JSON 文件(默认 <程序运行目录>/.cloudsync_state.json)
class SyncStateStore
{
public:
    explicit SyncStateStore(const QString& filePath = QString());

    bool load();
    bool save() const;

    QString filePath() const;
    QHash<QString, SyncRecord>& records() { return records_; }

private:
    QString filePath_;
    QHash<QString, SyncRecord> records_;
};
