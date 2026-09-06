#include "syncstate.h"
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>

SyncStateStore::SyncStateStore(const QString& filePath)
    : filePath_(filePath)
{
    if (filePath_.isEmpty())
    {
        // 默认放程序运行目录,便于调试查看
        filePath_ = QDir(QCoreApplication::applicationDirPath())
                        .filePath(".cloudsync_state.json");
    }
}

QString SyncStateStore::filePath() const { return filePath_; }

bool SyncStateStore::load()
{
    records_.clear();
    QFile f(filePath_);
    if (!f.exists())
        return true;                       // 首次使用,无状态文件不算错
    if (!f.open(QIODevice::ReadOnly))
        return false;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return false;
    f.close();

    const QJsonObject root = doc.object();
    for (auto it = root.begin(); it != root.end(); ++it)
    {
        const QJsonObject jo = it.value().toObject();
        SyncRecord r;
        r.localPath = it.key();
        r.cloudDir  = jo.value("cloudDir").toString();
        r.cloudName = jo.value("cloudName").toString();
        r.mtime     = jo.value("mtime").toDouble();
        r.size      = jo.value("size").toDouble();
        r.version   = jo.value("version").toInt(-1);

        const QJsonArray arr = jo.value("blocks").toArray();
        r.manifest.reserve(arr.size());
        for (const auto& v : arr)
        {
            const QJsonObject bo = v.toObject();
            SyncBlock b;
            b.hash   = bo.value("h").toString();
            b.offset = bo.value("o").toDouble();
            b.size   = bo.value("s").toDouble();
            if (!b.hash.isEmpty())
                r.manifest.push_back(b);
        }
        records_.insert(r.localPath, r);
    }
    return true;
}

bool SyncStateStore::save() const
{
    QJsonObject root;
    for (auto it = records_.cbegin(); it != records_.cend(); ++it)
    {
        const SyncRecord& r = it.value();
        QJsonObject jo;
        jo["cloudDir"]  = r.cloudDir;
        jo["cloudName"] = r.cloudName;
        jo["mtime"]     = double(r.mtime);
        jo["size"]      = double(r.size);
        jo["version"]   = r.version;

        QJsonArray arr;
        for (const auto& b : r.manifest)
        {
            QJsonObject bo;
            bo["h"] = b.hash;
            bo["o"] = double(b.offset);
            bo["s"] = double(b.size);
            arr.append(bo);
        }
        jo["blocks"] = arr;
        root[it.key()] = jo;
    }

    // QSaveFile:写完原子替换,避免中途崩溃损坏状态文件
    QSaveFile f(filePath_);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return f.commit();
}
