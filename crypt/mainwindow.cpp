#include "mainwindow.h"
#include "ui_mainwindow.h"
#include<QDebug>
#include<QMessageBox>
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)

{
    this->setFixedHeight(500);
    this->setFixedWidth(500);
    ui->setupUi(this);    
    _socket=new QTcpSocket(this);
    _socket->connectToHost("192.168.234.129",8001);
    _secmng=nullptr;
    connect(_socket,
            &QTcpSocket::readyRead,
            this,
            &MainWindow::onReadyRead);
    _token="";
    _clientId=0;
}

MainWindow::~MainWindow()
{
    delete ui;
    if(_secmng)
        delete _secmng;
    if(_index)
        delete _index;
}


void MainWindow::on_login_clicked()
{
    QString email = ui->lineEdit_email->text().trimmed();
    QString password = ui->lineEdit_password->text();

    if(email.isEmpty() || password.isEmpty())
    {
        QMessageBox::warning(this,
                             "提示",
                             "邮箱和密码不能为空");
        return;
    }

    LoginRequest req;
    req.set_email(email.toStdString());
    req.set_password(password.toStdString());

    QByteArray data(req.SerializeAsString().data(),
                    req.ByteSizeLong());

    _socket->write(data);
}

void MainWindow::onReadyRead()
{
    QByteArray buffer = _socket->readAll();

    LoginResponse rsp;

    if(!rsp.ParseFromArray(buffer.data(), buffer.size()))
    {
        QMessageBox::warning(this,
                             "错误",
                             "解析失败");
        return;
    }

    if(!rsp.status())
    {
        QMessageBox::warning(this,
                             "登录失败",
                             QString::fromStdString(rsp.message()));
        return;
    }

    // 保存token
    _token = QString::fromStdString(rsp.token());

    // 保存clientId
    _clientId = QString::fromStdString(rsp.clientid());

    // 开始密钥协商
    _secmng=new Secmng(this);
    _secmng->setToken(_token);
    _secmng->set_clientid(_clientId);
    connect(_secmng,
            &Secmng::agreeSuccess,
            this,
            [this]()
            {
                _index=new index(_secmng,_token,_clientId);
                _index->show();

                this->hide();
            });
    QMessageBox::information(this,
                         "成功",
                         "登陆成功");
}

