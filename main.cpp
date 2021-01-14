#include <QApplication>

#include "papermanager.h"


int main(int argc, char *argv[])
{

    auto mutexHandle = CreateMutexA(nullptr, false, "RippleWallpaper_Mutex");
    if (!mutexHandle || GetLastError() == ERROR_ALREADY_EXISTS)    // 使用互斥锁防止进程多开，该锁可跨进程获取
    {
        CloseHandle(mutexHandle);

        MessageBox(nullptr, L"程序已运行", L"提示", MB_ICONINFORMATION | MB_OK);

        return -1;
    }

    QApplication a(argc, argv);

    PaperManager* pmw=new PaperManager;

    return a.exec();


}
