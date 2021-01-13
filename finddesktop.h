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
        SendMessageTimeout(hWnd, 0x52C, 0, 0, SMTO_NORMAL, 1000, nullptr);

        HWND hDefView = NULL;
        HWND hWorkerW = FindWindowEx(NULL, NULL, L"WorkerW", NULL);

        while ((!hDefView) && hWorkerW)
        {
            hDefView = FindWindowEx(hWorkerW, NULL, L"SHELLDLL_DefView", NULL);
            hWorkerW = FindWindowEx(NULL, hWorkerW, L"WorkerW", NULL);
        }

        ShowWindow(hWorkerW,0);

        return hWnd;
    }

    static HWND findWorkerW()
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
