#ifndef PAPERMANAGER_H
#define PAPERMANAGER_H

#pragma execution_character_set("utf-8")

#include <QDialog>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QDialogButtonBox>

#include "ripplewindow.h"

//编写控制窗口，并管理一个ripplewindow实例
namespace Ui {
class PaperManager;
}

class PaperManager : public QDialog
{
    Q_OBJECT

public:
    explicit PaperManager(QWidget *parent = 0);
    ~PaperManager();

    void settingApply();//主要是点击“应用”按钮时调用，用来改变ripplewindow的参数设置

protected:
    void closeEvent(QCloseEvent* ev) override;//忽略关闭时间并改为隐藏，否则关闭窗口会导致应用关闭

    void initSetting();//这里其实被我用来设置qslider的区间了
    bool loadSettingFromFile();//从json文件中获取设置，可以获得用户之前的设置
    void saveSetting();//将当前设置保存为json文件"save.dat"
    void getCurSetting();//获取当前ripplewindow的参数，并根据参数修改qslider滑块的位置
    void setDefault();//一些情况下以及点击“恢复默认”时将ripplewindow的参数恢复为默认值,但是不会重设背景图片

    void createTrayIcon();//生成托盘图标及其菜单

    void onSettingActionTriggered();//"设置"选项触发
    void onExit();//"退出"选项触发
    void onButtonBoxClicked(QAbstractButton* button);//处理QDialogButtonBox事件
    void backgroundFileChoose();//"选择文件"按钮触发

private:
    Ui::PaperManager *ui;
    RippleWindow* m_ripplewindow;

    QMenu* m_trayIconMenu;
    QSystemTrayIcon* m_trayIcon;


};

#endif // PAPERMANAGER_H
