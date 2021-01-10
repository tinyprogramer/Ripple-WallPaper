#include "mainwindow.h"
#include <QApplication>
#include "ripplewindow.h"
#include "finddesktop.h"
#include <QtGui/QGuiApplication>
#include <QWindow>

int main(int argc, char *argv[])
{

    //QGuiApplication app(argc, argv);
    QApplication app(argc, argv);
    //MainWindow w;
    //w.show();
    //w.showFullScreen();

    //RippleWindow* rw=new RippleWindow();
    //rw->resize(1024,1024);
    //rw->show();

    //QWindow* w=new QWindow;


    HWND desk=FindDesktop::findDesk();
    QWindow* p=QWindow::fromWinId(reinterpret_cast<WId>(desk));


    RippleWindow* rw=new RippleWindow();
    rw->setParent(p);
    rw->resize(p->size());
    rw->setBackgroundImage("E:/qtproject/testglwaterripple/demo/img/bg3.jpg");
    rw->show();
    //rw->drop(500,500,20,0.01);
    //w->show();

    //openglwindow * ow=new openglwindow();
    //ow->resize(512,512);
    //ow->show();
    //openglwindow w;
    //w.showMaximized();

   //RippleWindow rw;
   //rw.resize(1024,1024);
   ////rw.showMaximized();
   //rw.show();

    return app.exec();
    qDebug()<<"after exec";
}
