#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ripplewindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    //RippleWidget* rw=new RippleWidget(this);
    //rw->setBackgroundImage("E:/qtproject/testglwaterripple/demo/img/bg3.jpg");
}

MainWindow::~MainWindow()
{
    delete ui;
}
