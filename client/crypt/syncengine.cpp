#include "syncengine.h"
#include "codec.h"
#include "myaes.h"
#include <QTcpSocket>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <cstring>

SyncEngine::SyncEngine(QObject* parent)
    : QObject(parent)
{
}

void SyncEngine::init(SyncStateStore* store) { store_ = store; }

void SyncEngine::setCredential(const QString& host, quint16 port,
                               const QString& token, const QString& clientId,
                               const QByteArray& aesKey)
{
    host_ = host;
    port_ = port;
    token_ = token;
    clientId_ = clientId;
    aesKey_ = aesKey;
}

// 登记一个刚普通上传成功的文件:云端位置 + 本地基线记录(空清单,version=-1)
void SyncEngine::registerUploadedFile(const QString& localPath,
                                      const QString& cloudDir,
                                      const QString& cloudName)
{
    if (localPath.isEmpty() || cloudName.isEmpty())
        return;

    SyncRecord& r = store_->records()[localPath];
    r.localPath  = localPath;
    r.cloudDir   = cloudDir.isEmpty() ? QStringLiteral("/") : cloudDir;
    r.cloudName  = cloudName;
    r.version    = -1;                      // 尚未建块版本
    r.manifest.clear();                     // 普通上传没有块清单基线
    const QFileInfo fi(localPath);
    r.mtime = fi.exists() ? fi.lastModified().toMSecsSinceEpoch() : 0;
    r.size  = fi.exists() ? fi.size() : 0;
    store_->save();
}

void SyncEngine::fillBase(FileRequest& req, CmdType type,
                          const QString& cloudDir, const QString& cloudName)
{
    req.set_type(type);
    req.set_token(token_.toStdString());
    req.set_clientid(clientId_.toStdString());
    req.set_path(cloudDir.toStdString());
    req.set_filename(cloudName.toStdString());
}

bool SyncEngine::sendRequest(FileRequest& req, FileResponse& rsp)
{
    QTcpSocket socket;
    socket.connectToHost(host_, port_);
    if (!socket.waitForConnected(3000))
        return false;

    const std::string packet = Codec::encode(req);
    if (socket.write(packet.data(), qint64(packet.size())) < 0)
        return false;
    if (!socket.waitForBytesWritten(3000))
        return false;

    QByteArray buf;
    while (true)
    {
        if (!socket.waitForReadyRead(3000))
            return false;
        buf.append(socket.readAll());
        if (buf.size() < 4)
            continue;

        uint32_t len;
        memcpy(&len, buf.constData(), 4);
        len = ntohl(len);
        if (len == 0 || buf.size() < 4 + int(len))
            continue;

        const QByteArray body = buf.mid(4, int(len));
        if (!rsp.ParseFromArray(body.data(), body.size()))
            continue;
        return true;
    }
}

// ================= SYNC_CHECK / SYNC_UPLOAD / SYNC_COMMIT =================

QStringList SyncEngine::serverMissingHashes(const SyncRecord& rec,
                                            const QStringList& candidateHashes,
                                            int* curVersion)
{
    QStringList missing;
    FileRequest req;
    fillBase(req, SYNC_CHECK, rec.cloudDir, rec.cloudName);
    for (const auto& h : candidateHashes)
        req.add_block_hashs(h.toStdString());

    FileResponse rsp;
    if (!sendRequest(req, rsp))
        return missing;

    if (curVersion)
        *curVersion = int(rsp.upload_id());       // 服务端当前块版本
    for (int i = 0; i < rsp.missing_hashes_size(); ++i)
        missing << QString::fromStdString(rsp.missing_hashes(i));
    return missing;
}

bool SyncEngine::uploadBlock(const QString& localPath, const SyncBlock& b)
{
    QFile f(localPath);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    if (!f.seek(b.offset))
        return false;
    const QByteArray plain = f.read(b.size);
    f.close();
    if (plain.size() != b.size)
        return false;

    MyAES aes(reinterpret_cast<const unsigned char*>(aesKey_.constData()));
    std::vector<unsigned char> cipher;
    if (!aes.encrypt(reinterpret_cast<const unsigned char*>(plain.constData()),
                     int(plain.size()), cipher))
        return false;

    const QByteArray iv(reinterpret_cast<const char*>(aes.getIV()), IV_SIZE);
    const QByteArray cipherData(reinterpret_cast<const char*>(cipher.data()),
                                int(cipher.size()));

    FileRequest req;
    fillBase(req, SYNC_UPLOAD, QString(), QString());   // 单块上传不需要路径
    req.set_block_hash(b.hash.toStdString());
    req.set_iv(iv.constData(), iv.size());
    req.set_data(cipherData.constData(), cipherData.size());

    FileResponse rsp;
    return sendRequest(req, rsp) && rsp.status();
}

int SyncEngine::commitManifest(const SyncRecord& rec,
                               const QVector<SyncBlock>& manifest,
                               int curVersion)
{
    FileRequest req;
    fillBase(req, SYNC_COMMIT, rec.cloudDir, rec.cloudName);
    req.set_version(uint64_t(curVersion));            // 乐观锁
    for (const auto& b : manifest)
    {
        req.add_block_hashs(b.hash.toStdString());
        req.add_block_size(uint32_t(b.size));
    }

    FileResponse rsp;
    if (!sendRequest(req, rsp) || !rsp.status())
        return -1;
    return int(rsp.upload_id());                      // 服务端返回新版本号
}

// ================= 本地处理 =================

QVector<SyncBlock> SyncEngine::chunkLocalFile(const QString& localPath)
{
    QVector<SyncBlock> list;
    QFile f(localPath);
    if (!f.open(QIODevice::ReadOnly))
        return list;

    // TODO: 大文件整读进内存,后续可改流式 CDC
    const QByteArray data = f.readAll();
    f.close();

    const std::vector<CdcBlock> blocks =
        chunker_.chunkAll(data.constData(), size_t(data.size()));
    list.reserve(int(blocks.size()));
    for (const auto& b : blocks)
        list.push_back({ QString::fromStdString(b.hash),
                         qint64(b.offset), qint64(b.size) });
    return list;
}

void SyncEngine::syncFile(const QString& localPath)
{
    auto it = store_->records().find(localPath);
    if (it == store_->records().end())
        return;                                    // 没登记过(非本客户端上传),忽略

    SyncRecord& rec = it.value();
    const QFileInfo fi(localPath);
    if (!fi.exists() || !fi.isFile())
    {
        emit syncFinished(localPath, false);
        return;
    }

    // 0. mtime/size 没变 → 无需同步(首次登记后未改动也不动)
    if (rec.mtime == fi.lastModified().toMSecsSinceEpoch()
        && rec.size == fi.size())
        return;

    // 1. 重跑 CDC → 新清单
    const QVector<SyncBlock> newManifest = chunkLocalFile(localPath);
    if (newManifest.isEmpty())
    {
        emit syncFinished(localPath, false);
        return;
    }

    // 2. 与本地旧清单差集(普通上传后旧清单为空 → 全量块都算"变化")
    QSet<QString> oldHashes;
    for (const auto& b : rec.manifest)
        oldHashes.insert(b.hash);

    QVector<SyncBlock> changed;
    for (const auto& b : newManifest)
        if (!oldHashes.contains(b.hash))
            changed.push_back(b);

    if (changed.isEmpty())
    {
        updateRecord(localPath, rec.version);      // 只刷新 mtime
        emit syncFinished(localPath, true);
        return;
    }

    // 3. SYNC_CHECK:拿真缺的块 + 当前版本
    QStringList cand;
    for (const auto& b : changed)
        cand << b.hash;
    int curVersion = 0;
    const QStringList missing = serverMissingHashes(rec, cand, &curVersion);

    // 4. 串行传缺失块
    int done = 0;
    for (const auto& b : changed)
    {
        if (!missing.contains(b.hash))
        {
            ++done;
            continue;
        }
        if (!uploadBlock(localPath, b))
        {
            emit syncProgress(localPath, done, int(changed.size()));
            emit syncFinished(localPath, false);
            return;
        }
        ++done;
        emit syncProgress(localPath, done, int(changed.size()));
    }

    // 5. COMMIT(整份清单 + 乐观锁版本)
    const int newVersion = commitManifest(rec, newManifest, curVersion);
    if (newVersion < 0)
    {
        emit syncFinished(localPath, false);
        return;
    }

    // 6. 更新记录:新清单 + 新版本 + mtime
    rec.manifest = newManifest;
    updateRecord(localPath, newVersion);
    emit syncFinished(localPath, true);
}

void SyncEngine::updateRecord(const QString& localPath, int newVersion)
{
    auto it = store_->records().find(localPath);
    if (it == store_->records().end())
        return;
    SyncRecord& r = it.value();
    r.version = newVersion;
    const QFileInfo fi(localPath);
    r.mtime = fi.lastModified().toMSecsSinceEpoch();
    r.size  = fi.size();
    store_->save();
}
