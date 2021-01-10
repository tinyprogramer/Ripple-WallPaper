#ifndef FINDDESKTOP_H
#define FINDDESKTOP_H

#include <Windows.h>
#pragma comment(lib,"user32.lib")

#include <QDebug>

class FindDesktop
{
public:
    static HWND findDesk()
    {
        auto hWnd = FindWindow(L"Progman", L"Program Manager");
        SendMessageTimeout(hWnd, 0x52C, 0, 0, SMTO_NORMAL, 1000, nullptr);	// 不知道是否可以为空指针

        //HWND hWndWorkW = nullptr;
        //do
        //{
        //    hWndWorkW = FindWindowEx(nullptr, hWndWorkW, L"WorkerW", nullptr);
        //    if (hWndWorkW)
        //    {
        //        if (FindWindowEx(hWndWorkW, nullptr, L"SHELLDLL_DefView", nullptr))
        //        {
        //            auto h = FindWindowEx(nullptr, hWndWorkW, L"WorkerW", nullptr);
        //            while (h)
        //            {
        //                SendMessage(h, WM_CLOSE, 0, 0);
        //                h = FindWindowEx(nullptr, hWndWorkW, L"WorkerW", nullptr);
        //            }
        //
        //            break;
        //        }
        //    }
        //} while (true);
        HWND hDefView = NULL;
        HWND hWorkerW = FindWindowEx(NULL, NULL, L"WorkerW", NULL);
        //HWND WorkerW =NULL;
        //通过遍历找到包含SHELLDLL_DefView的WorkerW
        while ((!hDefView) && hWorkerW)
        {
            hDefView = FindWindowEx(hWorkerW, NULL, L"SHELLDLL_DefView", NULL);
            //WorkerW=hWorkerW;//得到WorkerW
            hWorkerW = FindWindowEx(NULL, hWorkerW, L"WorkerW", NULL);
        }
        //隐藏窗口，不让mainwindow被挡住
        ShowWindow(hWorkerW,0);
        //return FindWindow(L"Progman",NULL);

        return hWnd;
    }

    static HWND findWorkerW()
    {
        HWND hDefView = NULL;
        HWND hWorkerW = FindWindowEx(NULL, NULL, L"WorkerW", NULL);
        HWND WorkerW =NULL;
        //通过遍历找到包含SHELLDLL_DefView的WorkerW
        while ((!hDefView) && hWorkerW)
        {
            hDefView = FindWindowEx(hWorkerW, NULL, L"SHELLDLL_DefView", NULL);
            WorkerW=hWorkerW;//得到WorkerW
            hWorkerW = FindWindowEx(NULL, hWorkerW, L"WorkerW", NULL);
        }
        //隐藏窗口，不让mainwindow被挡住
        //ShowWindow(hWorkerW,0);
        return WorkerW;
    }
};


#endif // FINDDESKTOP_H
