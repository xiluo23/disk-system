#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    cap.open(0);
    if(!cap.isOpened()){
        qDebug()<<"fail to open cap";
        return ;
    }
    cv::Mat img;
    if(!cap.read(img)){
        qDebug()<<"read img fail";
        return ;
    }
    cv::imwrite("./test.jpg",img);

}

MainWindow::~MainWindow()
{
    delete ui;
}
