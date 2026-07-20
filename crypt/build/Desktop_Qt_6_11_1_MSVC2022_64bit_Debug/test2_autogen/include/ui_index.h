/********************************************************************************
** Form generated from reading UI file 'index.ui'
**
** Created by: Qt User Interface Compiler version 6.11.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_INDEX_H
#define UI_INDEX_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_index
{
public:
    QWidget *centralwidget;
    QVBoxLayout *verticalLayout;
    QGroupBox *groupBox;
    QHBoxLayout *horizontalLayout;
    QPushButton *upLoad;
    QPushButton *createdirButton;
    QPushButton *backButton;
    QSpacerItem *horizontalSpacer;
    QLabel *pathLabel;
    QTableWidget *tableWidget;
    QMenuBar *menubar;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *index)
    {
        if (index->objectName().isEmpty())
            index->setObjectName("index");
        index->resize(800, 600);
        centralwidget = new QWidget(index);
        centralwidget->setObjectName("centralwidget");
        verticalLayout = new QVBoxLayout(centralwidget);
        verticalLayout->setObjectName("verticalLayout");
        groupBox = new QGroupBox(centralwidget);
        groupBox->setObjectName("groupBox");
        horizontalLayout = new QHBoxLayout(groupBox);
        horizontalLayout->setObjectName("horizontalLayout");
        upLoad = new QPushButton(groupBox);
        upLoad->setObjectName("upLoad");

        horizontalLayout->addWidget(upLoad);

        createdirButton = new QPushButton(groupBox);
        createdirButton->setObjectName("createdirButton");

        horizontalLayout->addWidget(createdirButton);

        backButton = new QPushButton(groupBox);
        backButton->setObjectName("backButton");

        horizontalLayout->addWidget(backButton);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);


        verticalLayout->addWidget(groupBox);

        pathLabel = new QLabel(centralwidget);
        pathLabel->setObjectName("pathLabel");

        verticalLayout->addWidget(pathLabel);

        tableWidget = new QTableWidget(centralwidget);
        tableWidget->setObjectName("tableWidget");

        verticalLayout->addWidget(tableWidget);

        index->setCentralWidget(centralwidget);
        menubar = new QMenuBar(index);
        menubar->setObjectName("menubar");
        menubar->setGeometry(QRect(0, 0, 800, 21));
        index->setMenuBar(menubar);
        statusbar = new QStatusBar(index);
        statusbar->setObjectName("statusbar");
        index->setStatusBar(statusbar);

        retranslateUi(index);

        QMetaObject::connectSlotsByName(index);
    } // setupUi

    void retranslateUi(QMainWindow *index)
    {
        index->setWindowTitle(QCoreApplication::translate("index", "MainWindow", nullptr));
        groupBox->setTitle(QString());
        upLoad->setText(QCoreApplication::translate("index", "\344\270\212\344\274\240\346\226\207\344\273\266", nullptr));
        createdirButton->setText(QCoreApplication::translate("index", "\345\210\233\345\273\272\346\226\207\344\273\266\345\244\271", nullptr));
        backButton->setText(QCoreApplication::translate("index", "\350\277\224\345\233\236", nullptr));
        pathLabel->setText(QString());
    } // retranslateUi

};

namespace Ui {
    class index: public Ui_index {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_INDEX_H
