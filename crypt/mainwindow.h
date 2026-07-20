#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include"Secmng.h"
#include"index.h"
#include<QRandomGenerator>
#include"Login.pb.h"
QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onReadyRead();

    void on_login_clicked();

private:
    Ui::MainWindow *ui;
    index*_index;
    Secmng* _secmng;//密钥协商
    QTcpSocket*_socket;
    QString _token;
    QString _clientId;

};
#endif // MAINWINDOW_H
