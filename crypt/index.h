#ifndef INDEX_H
#define INDEX_H
#include "ui_index.h"
#include"File.pb.h"
#include"codec.h"
#include<QFile>
#include<QFileDialog>
#include <QMainWindow>
#include<QTcpSocket>
#include"secmng.h"
#include<openssl/md5.h>
#include<QMessageBox>
#include <QCryptographicHash>
#include<QInputDialog>
#include<QProgressBar>
const int CHUNK_SIZE=1024*1024; //1MB

namespace Ui {
class index;
}

class index : public QMainWindow
{
    Q_OBJECT

public:
    explicit index(Secmng*&secmng,QString&token,QString&clientid,QWidget *parent = nullptr);
    ~index();
    void handle_upload(const FileResponse&);
    void handleList(const FileResponse&);
    void handle_delete(const FileResponse&);
    void handle_download(const FileResponse&);
    void handle_rename(const FileResponse&);
    void handle_mkdir(const FileResponse&);
    void handle_uploadCheck(const FileResponse&);


    QString calcFileMd5(const QByteArray&);
    void downloadFile(int );
    void deleteFile(int);
    void renameFile(int);


    void sendNextChunk();
    QString calcMd5(const QByteArray&);
    QString calcMd5(const QString&);
private slots:
    void onReadyRead();
    void showContextMenu(const QPoint& pos);
    void on_FileList();
    void on_upLoad_clicked();
    void on_createdirButton_clicked();
    void on_backButton_clicked();
    void onTableDoubleClicked(int );
    void updatePath();
private:
    Ui::index *ui;
    Secmng* _secmng;//密钥协商
    QTcpSocket*_socket;
    QString _token;
    QString _clientId;
    QByteArray recvBuffer_;
    QString _currentPath;

    QProgressBar*_uploadProgress;
    quint64 _uploadTotalSize ;
    quint64 _uploadBytes;
    QString _uploadFileName;
    QFile* _uploadFile;
    quint64 _uploadOffset;
    QString _filemd5;
};

#endif // INDEX_H
