#include "index.h"

index::index(Secmng*&secmng,QString&token,QString&clientid,QWidget *parent)
    : QMainWindow(parent),_secmng(secmng),_token(token),_clientId(clientid)
    , ui(new Ui::index),_currentPath("/"),_uploadProgress(nullptr),
    _uploadBytes(0),_uploadTotalSize(0),_uploadFile(nullptr)
{
    this->setWindowTitle("FileDisk");
    this->setFixedHeight(600);
    this->setFixedWidth(800);
    ui->setupUi(this);
    ui->tableWidget->setColumnCount(3);
    QStringList headers;
    headers << "文件名"
            <<"类型"
            << "大小";

    ui->tableWidget->setHorizontalHeaderLabels(headers);
    auto header = ui->tableWidget->horizontalHeader();

    header->setSectionResizeMode(
        0,
        QHeaderView::Stretch);

    header->setSectionResizeMode(
        1,
        QHeaderView::ResizeToContents);

    header->setSectionResizeMode(
        2,
        QHeaderView::ResizeToContents);

    ui->tableWidget->setEditTriggers(
        QAbstractItemView::NoEditTriggers
        );

    // 整行选择
    ui->tableWidget->setSelectionBehavior(
        QAbstractItemView::SelectRows
        );
    // 开启右键菜单
    ui->tableWidget->setContextMenuPolicy(
        Qt::CustomContextMenu
        );

    ui->pathLabel->setText(_currentPath);
    connect(
        ui->tableWidget,
        &QTableWidget::customContextMenuRequested,
        this,
        &index::showContextMenu
        );

    _socket=new QTcpSocket(this);
    _socket->connectToHost("192.168.234.129",8003);
    connect(_socket,
            &QTcpSocket::readyRead,
            this,
            &index::onReadyRead);
    connect(_socket,&QTcpSocket::connected,this,&index::on_FileList);
    connect(ui->tableWidget,
            &QTableWidget::cellDoubleClicked,
            this,
            &index::onTableDoubleClicked);
    updatePath();
    _uploadProgress=new QProgressBar;
}
QString fileType(const QString name,bool isDir){
    if (isDir)
        return "文件夹";

    QString suffix =
        QFileInfo(name).suffix().toLower();

    if (suffix == "txt")
        return "文本文档";

    if (suffix == "pdf")
        return "PDF文档";

    if (suffix == "doc" || suffix == "docx")
        return "Word文档";

    if (suffix == "xls" || suffix == "xlsx")
        return "Excel文档";

    if (suffix == "ppt" || suffix == "pptx")
        return "PPT文档";

    if (suffix == "png")
        return "PNG图片";

    if (suffix == "jpg" || suffix == "jpeg")
        return "JPEG图片";

    if (suffix == "gif")
        return "GIF图片";

    if (suffix == "bmp")
        return "BMP图片";

    if (suffix == "zip" || suffix == "rar" || suffix == "7z")
        return "压缩文件";

    if (suffix == "mp3")
        return "MP3音频";

    if (suffix == "mp4")
        return "MP4视频";

    if (suffix == "cpp")
        return "C++源文件";

    if (suffix == "h")
        return "头文件";

    if (suffix == "py")
        return "Python脚本";

    if (suffix == "ipynb")
        return "Jupyter Notebook";

    return "未知文件";
}
void index::updatePath()
{
    ui->pathLabel->setText("当前路径：" + _currentPath);
}
void index::onTableDoubleClicked(int row)
{
    bool isDir =
        ui->tableWidget
            ->item(row,0)
            ->data(Qt::UserRole+1)
            .toBool();

    if(!isDir)
        return;

    QString name =
        ui->tableWidget
            ->item(row,0)
            ->text();
    if(_currentPath.back()!='/')_currentPath+="/";
    _currentPath +=name;
    updatePath();
    on_FileList();
}

QString formatSize(uint64_t size)
{
    double s=size;

    QStringList units{
        "B",
        "KB",
        "MB",
        "GB"
    };

    int index=0;

    while(s>=1024 && index<3)
    {
        s/=1024;
        index++;
    }


    return QString::number(s,'f',2)
           +" "
           +units[index];
}
index::~index()
{
    delete ui;
}
void index::handle_upload(const FileResponse&rsp){
    if (rsp.status())
    {
        _uploadBytes+=rsp.chunk_size();
        int value =
            static_cast<int>(
                (_uploadBytes * 100)
                /
                _uploadTotalSize
                );
        _uploadOffset += rsp.chunk_size();
        _uploadProgress->setValue(value);
        if(rsp.eof()){
            QMessageBox::information(this,
                                 "上传成功",
                                 QString::fromStdString(rsp.message()));
            _uploadProgress->hide();
            _uploadFile->close();
            // 刷新文件列表
            on_FileList();
        }
        else{
            sendNextChunk();
        }
    }
    else
    {
        qDebug()<<"this chunk send fail";
        sendNextChunk();
    }
}

void index::onReadyRead(){
    FileResponse rsp;
    recvBuffer_.append(_socket->readAll());
    while (true)
    {
        if (recvBuffer_.size() < 4)
            return;

        uint32_t len;
        memcpy(&len, recvBuffer_.constData(), 4);
        len = ntohl(len);

        if (recvBuffer_.size() < 4 + len)
            return;

        QByteArray body = recvBuffer_.mid(4, len);

        recvBuffer_.remove(0, 4 + len);

        FileResponse rsp;
        if (!rsp.ParseFromArray(body.data(), body.size()))
            continue;
        switch(rsp.type()){
            case UPLOAD_FILE:
                handle_upload(rsp);
                break;
            case LIST_FILE:
                handleList(rsp);
                break;
            case DELETE_FILE:
                handle_delete(rsp);
                break;
            case DOWNLOAD_FILE:
                handle_download(rsp);
                break;
            case RENAME_FILE:
                handle_rename(rsp);
                break;
            case MKDIR:
                handle_mkdir(rsp);
                break;
            case UPLOAD_CHECK:
                handle_uploadCheck(rsp);
                break;
            default:
                break;
        }
    }
}
void index::handle_rename(const FileResponse&rsp){
    if(rsp.status())
    {
        QMessageBox::information(
            this,
            "提示",
            "修改成功"
            );

        on_FileList();
    }
    else
    {
        QMessageBox::warning(
            this,
            "错误",
            QString::fromStdString(
                rsp.message()
                )
            );
    }
}
void index::handle_download(const FileResponse&rsp){
    if (!rsp.status())
    {
        QMessageBox::warning(
            this,
            "下载失败",
            QString::fromStdString(rsp.message()));
        return;
    }

    QByteArray cipher(rsp.data().data(), rsp.data().size());
    QByteArray plain;

    if (!_secmng->decrypt(cipher, plain))
    {
        QMessageBox::warning(
            this,
            "错误",
            "AES解密失败");
        return;
    }

    QString savePath =
        QFileDialog::getSaveFileName(
            this,
            "保存文件",
            QString::fromStdString(rsp.files(0).filename()));

    if (savePath.isEmpty())
        return;

    QFile file(savePath);

    if (!file.open(QIODevice::WriteOnly))
    {
        QMessageBox::warning(
            this,
            "错误",
            "无法创建文件");
        return;
    }

    file.write(plain);
    file.close();

    QMessageBox::information(
        this,
        "提示",
        "下载成功");
}
void index::handle_uploadCheck(const FileResponse&rsp){
    if(rsp.status()){
        _uploadProgress->setValue(100);
        _uploadProgress->hide();
        QMessageBox::warning(this,
                             "上传成功",
                             QString::fromStdString(rsp.message()));
        _uploadFile->close();
        on_FileList();
    }
    else{
        _uploadBytes=rsp.offset();
        _uploadOffset=rsp.offset();
        int value =
            static_cast<int>(
                (_uploadBytes * 100)
                /
                _uploadTotalSize
                );
        _uploadProgress->setValue(value);
        _uploadProgress->show();
        sendNextChunk();
    }
}
void index::on_upLoad_clicked()
{
    QString fileName =
        QFileDialog::getOpenFileName(
            this,
            "选择文件"
            );
    if(fileName.isEmpty())
        return;
    _uploadFile =
        new QFile(fileName,this);
    if(!_uploadFile->open(QIODevice::ReadOnly))
    {
        delete _uploadFile;
        _uploadFile=nullptr;
        return;
    }
    _uploadFileName =
        QFileInfo(fileName).fileName();
    _uploadTotalSize =_uploadFile->size();
    _uploadOffset=0;
    _uploadBytes=0;
    _uploadProgress->setValue(0);
    // 先检查是否秒传
    QString file_md5=calcMd5(fileName);
    _filemd5=file_md5;
    FileRequest req;
    req.set_iv(_secmng->getIV());
    req.set_type(UPLOAD_CHECK);
    req.set_path(_currentPath.toStdString());
    req.set_token(_token.toStdString());
    req.set_md5(file_md5.toStdString());
    req.set_clientid(_clientId.toStdString());
    req.set_filename(_uploadFileName.toStdString());
    req.set_filesize(_uploadTotalSize);
    std::string packet =
        Codec::encode(req);


    _socket->write(
        packet.data(),
        packet.size()
        );

}
QString index::calcMd5(const QString& filePath)
{
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly))
    {
        qDebug() << "open file failed";
        return "";
    }


    MD5_CTX ctx;

    MD5_Init(&ctx);


    const int BUFFER_SIZE = 1024 * 1024; // 1MB

    while (!file.atEnd())
    {
        QByteArray buffer =
            file.read(BUFFER_SIZE);


        if (buffer.isEmpty())
            break;


        MD5_Update(
            &ctx,
            buffer.constData(),
            buffer.size()
            );
    }


    unsigned char result[MD5_DIGEST_LENGTH];


    MD5_Final(
        result,
        &ctx
        );


    file.close();


    QString md5;


    for (int i = 0; i < MD5_DIGEST_LENGTH; i++)
    {
        md5 += QString("%1")
        .arg(
            result[i],
            2,
            16,
            QLatin1Char('0')
            );
    }


    return md5;
}


void index::sendNextChunk()
{
    if(!_uploadFile)
        return;
    QByteArray plain =
        _uploadFile->read(CHUNK_SIZE);
    if(plain.isEmpty())
    {
        return;
    }

    QByteArray cipher;

    _secmng->encrypt(
        plain,
        cipher
        );

    FileRequest req;

    QString chunk_md5=calcMd5(plain);
    req.set_chunk_md5(chunk_md5.toStdString());
    req.set_md5(_filemd5.toStdString());
    req.set_type(UPLOAD_FILE);
    req.set_clientid(
        _clientId.toStdString()
        );

    req.set_token(
        _token.toStdString()
        );

    req.set_filename(
        _uploadFileName.toStdString()
        );


    req.set_path(
        _currentPath.toStdString()
        );


    req.set_filesize(
        _uploadTotalSize
        );


    req.set_offset(
        _uploadOffset
        );


    req.set_chunk_size(
        plain.size()
        );

    req.set_iv(_secmng->getIV());
    bool is_eof=(_uploadBytes+plain.size()==_uploadTotalSize);
    req.set_eof(is_eof);

    req.set_data(
        cipher.data(),
        cipher.size()
        );

    std::string packet =
        Codec::encode(req);


    _socket->write(
        packet.data(),
        packet.size()
        );
}


void index::on_FileList(){
    FileRequest req;

    req.set_type(LIST_FILE);

    // 用户身份
    req.set_token(_token.toStdString());

    req.set_clientid(_clientId.toStdString());
    req.set_path(_currentPath.toStdString());
    // 当前目录
    // 空表示根目录
    req.set_filename("");

    std::string packet = Codec::encode(req);

    _socket->write(packet.data(),
                   packet.size());

    qDebug()<<"request file list";

}
QString index::calcMd5(const QByteArray&plain){
    qDebug()<<plain.toHex();
    unsigned char md[MD5_DIGEST_LENGTH];
    MD5((const unsigned char*)plain.data(),plain.size(),md);
    char buf[33];
    for(int i = 0; i < MD5_DIGEST_LENGTH; ++i)
    {
        sprintf(buf + i * 2, "%02x", md[i]);
    }

    buf[32] = '\0';
    return QString(buf);
}

void index::showContextMenu(
    const QPoint& pos)
{

    QModelIndex index =
        ui->tableWidget
            ->indexAt(pos);


    if(!index.isValid())
        return;



    int row=index.row();



    QString filename =
        ui->tableWidget
            ->item(row,0)
            ->text();



    QMenu menu(this);



    QAction* download =
        menu.addAction("下载");


    QAction* rename =
        menu.addAction("重命名");


    QAction* remove =
        menu.addAction("删除");



    QAction* action =
        menu.exec(
            ui->tableWidget
                ->viewport()
                ->mapToGlobal(pos)
            );

    if(action==download)
    {
        downloadFile(row);
    }
    else if(action==rename)
    {
        renameFile(row);
    }
    else if(action==remove)
    {
        deleteFile(row);
    }
}
void index::handleList(const FileResponse& rsp)
{
    if(!rsp.status())
    {
        QMessageBox::warning(
            this,
            "错误",
            QString::fromStdString(rsp.message())
            );

        return;
    }
    qDebug()<<"recv handleList size :"<<rsp.files().size();
    ui->tableWidget->clearContents();

    ui->tableWidget->setRowCount(rsp.files_size());


    for(int i=0;i<rsp.files_size();i++)
    {
        const FileItem& item=rsp.files(i);


        auto nameItem =
            new QTableWidgetItem(
                QString::fromStdString(
                    item.filename()
                    )
                );

        auto typeItem=
            new QTableWidgetItem(
                fileType(
                    QString::fromStdString(item.filename()),
                    item.isdir()
                    )
                );


        auto sizeItem =
            new QTableWidgetItem(
                formatSize(
                    item.filesize()
                    )
                );
        if(item.isdir()){
            sizeItem->setText("--");
        }

        // 保存文件信息
        nameItem->setData(
            Qt::UserRole,
            QString::fromStdString(
                item.filename()
                )
            );


        nameItem->setData(
            Qt::UserRole+1,
            item.isdir()
            );



        ui->tableWidget->setItem(
            i,
            0,
            nameItem
            );


        ui->tableWidget->setItem(
            i,
            1,
            typeItem
            );
        ui->tableWidget->setItem(
            i,
            2,
            sizeItem
            );

    }
}


void index::downloadFile(int row)
{
    QString filename =
        ui->tableWidget
            ->item(row,0)
            ->text();


    qDebug()
        <<"download:"
        <<filename;


    FileRequest req;

    req.set_type(DOWNLOAD_FILE);

    req.set_token(
        _token.toStdString()
        );

    req.set_clientid(
        _clientId.toStdString()
        );
    req.set_path(_currentPath.toStdString());
    req.set_filename(
        filename.toStdString()
        );
    req.set_iv(_secmng->getIV());

    std::string packet =
        Codec::encode(req);


    _socket->write(
        packet.data(),
        packet.size()
        );
}


void index::deleteFile(int row)
{
    QString filename =
        ui->tableWidget
            ->item(row,0)
            ->text();


    auto ret = QMessageBox::question(
        this,
        "确认",
        "是否删除文件:\n"+filename
        );


    if(ret != QMessageBox::Yes)
        return;



    FileRequest req;


    req.set_type(DELETE_FILE);


    req.set_token(
        _token.toStdString()
        );


    req.set_clientid(
        _clientId.toStdString()
        );
    req.set_path(_currentPath.toStdString());

    req.set_filename(
        filename.toStdString()
        );


    std::string packet =
        Codec::encode(req);


    _socket->write(
        packet.data(),
        packet.size()
        );
}

void index::handle_delete(const FileResponse&rsp){
    if(rsp.status())
    {
        QMessageBox::information(
            this,
            "提示",
            "删除成功"
            );

        on_FileList();
    }
    else
    {
        QMessageBox::warning(
            this,
            "错误",
            QString::fromStdString(
                rsp.message()
                )
            );
    }
}
void index::renameFile(int row)
{
    QString oldName =
        ui->tableWidget
            ->item(row,0)
            ->text();


    QInputDialog dialog(this);

    dialog.setWindowTitle("重命名");

    dialog.setLabelText("请输入新名字:");

    dialog.setTextValue(oldName);

    dialog.resize(300,120);


    if(dialog.exec()!=QDialog::Accepted)
        return;


    QString newName =
        dialog.textValue();


    if(newName.isEmpty()
        || newName==oldName)
        return;


    FileRequest req;

    req.set_type(RENAME_FILE);

    req.set_filename(
        oldName.toStdString()
        );

    req.set_data(
        newName.toStdString()
        );
    req.set_path(_currentPath.toStdString());

    req.set_token(
        _token.toStdString()
        );


    req.set_clientid(
        _clientId.toStdString()
        );


    std::string data =
        Codec::encode(req);


    _socket->write(
        data.data(),
        data.size()
        );
}
void index::handle_mkdir(const FileResponse&rsp){
    if(rsp.status())
    {
        QMessageBox::information(
            this,
            "提示",
            "创建成功"
            );
    }
    else
    {
        QMessageBox::warning(
            this,
            "错误",
            QString::fromStdString(
                rsp.message()
                )
            );
    }
    on_FileList();
}

void index::on_createdirButton_clicked()
{
    QString dirName =
        QInputDialog::getText(
            this,
            "新建文件夹",
            "请输入文件夹名称："
            );

    if(dirName.isEmpty())
        return;

    FileRequest req;

    req.set_type(MKDIR);

    req.set_clientid(_clientId.toStdString());

    req.set_token(_token.toStdString());

    req.set_filename(dirName.toStdString());

    req.set_path(_currentPath.toStdString());

    std::string data = Codec::encode(req);

    _socket->write(data.data(), data.size());
}


void index::on_backButton_clicked()
{
    if(_currentPath=="/")
        return;

    int pos = _currentPath.lastIndexOf('/');

    _currentPath =_currentPath.left(pos);
    if(_currentPath.isEmpty())_currentPath="/";
    updatePath();

    on_FileList();
}

