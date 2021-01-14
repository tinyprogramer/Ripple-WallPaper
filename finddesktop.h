#ifndef FINDDESKTOP_H
#define FINDDESKTOP_H

#pragma execution_character_set("utf-8")
#pragma comment(lib,"user32.lib")

#include <Windows.h>

//参考了一些文章和代码，其中有一篇提到了可能是根源的出处
//https://www.codeproject.com/Articles/856020/Draw-Behind-Desktop-Icons-in-Windows-plus 这篇文章解释了在桌面绘制的原理
class FindDesktop
{
public:
    static HWND findDesk()
    {
        auto hWnd = FindWindow(L"Progman", L"Program Manager");//这里获得桌面句柄，但此时桌面和桌面图标是混合的
        //如果不加下方的代码，绘制会把桌面图标覆盖掉
        SendMessageTimeout(hWnd, 0x52C, 0, 0, SMTO_NORMAL, 1000, nullptr);//这一句会使windows系统在图标和桌面背景之间添加一层窗口
        //这个被添加的窗口名为WorkerW

        HWND hDefView = NULL;
        HWND hWorkerW = FindWindowEx(NULL, NULL, L"WorkerW", NULL);

        while ((!hDefView) && hWorkerW)//因为名为WorkerW的窗口不止一个，用下方的代码可以找到我们需要的WorkerW
        {
            hDefView = FindWindowEx(hWorkerW, NULL, L"SHELLDLL_DefView", NULL);
            hWorkerW = FindWindowEx(NULL, hWorkerW, L"WorkerW", NULL);
        }

        ShowWindow(hWorkerW,0);//这里跟链接的文章提到的不太一样，我参考的几份代码要么是把WorkerW关掉，要么是像这里一样隐藏
        //而文章中是获得WorkerW的句柄并在其上绘制，我自己测试时，感觉隐藏或关掉WorkerW的方法表现比较稳定

        return hWnd;//隐藏掉WorkerW后，我们需要的句柄仍然是桌面句柄，即此函数第一行获得的句柄
    }

    static HWND findWorkerW()//这个方法可以获得新生成的WorkerW窗口句柄，不过最终并没有使用这个句柄，代码姑且留着吧
    {
        HWND hDefView = NULL;
        HWND hWorkerW = FindWindowEx(NULL, NULL, L"WorkerW", NULL);
        HWND WorkerW =NULL;

        while ((!hDefView) && hWorkerW)
        {
            hDefView = FindWindowEx(hWorkerW, NULL, L"SHELLDLL_DefView", NULL);
            WorkerW=hWorkerW;//得到WorkerW
            hWorkerW = FindWindowEx(NULL, hWorkerW, L"WorkerW", NULL);
        }

        return WorkerW;
    }
};


#endif // FINDDESKTOP_H
