#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QVector>
#include "syncstate.h"
#include "cdc.h"
#include "File.pb.h"

// 增量同步引擎:跟踪"用户通过普通上传传过的文件",
// 上传成功后由界面调用 registerUploadedFile(),之后文件被修改时调用 syncFile()。
class SyncEngine : public QObject
{
    Q_OBJECT
public:
    explicit SyncEngine(QObject* parent = nullptr);

    void init(SyncStateStore* store);
    void setCredential(const QString& host, quint16 port,
                       const QString& token, const QString& clientId,
                       const QByteArray& aesKey);

    // 普通上传成功后登记该本地文件(云端目录/文件名 + 基线记录)
    // 界面应随后把 localPath 加进 QFileSystemWatcher
    void registerUploadedFile(const QString& localPath,
                              const QString& cloudDir,
                              const QString& cloudName);

    // 本地文件被修改后调用:重切 CDC → 差集 → 传新块 → COMMIT
    void syncFile(const QString& localPath);

signals:
    void syncProgress(const QString& localPath, int done, int total);
    void syncFinished(const QString& localPath, bool ok);

private:
    QVector<SyncBlock> chunkLocalFile(const QString& localPath);
    void updateRecord(const QString& localPath, int newVersion);

    // 网络原语
    QStringList serverMissingHashes(const SyncRecord& rec,
                                    const QStringList& candidateHashes,
                                    int* curVersion);
    bool uploadBlock(const QString& localPath, const SyncBlock& b);
    int  commitManifest(const SyncRecord& rec,
                        const QVector<SyncBlock>& manifest,
                        int curVersion);
    bool sendRequest(FileRequest& req, FileResponse& rsp);
    void fillBase(FileRequest& req, CmdType type,
                  const QString& cloudDir, const QString& cloudName);

private:
    SyncStateStore* store_ = nullptr;
    QString host_;
    quint16 port_ = 8003;
    QString token_;
    QString clientId_;
    QByteArray aesKey_;
    CdcChunker chunker_{128 * 1024, 32 * 1024, 512 * 1024};
};
