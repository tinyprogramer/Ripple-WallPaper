#ifndef PAPERMANAGER_H
#define PAPERMANAGER_H

#include <QDialog>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QDialogButtonBox>

#include "ripplewindow.h"

namespace Ui {
class PaperManager;
}

class PaperManager : public QDialog
{
    Q_OBJECT

public:
    explicit PaperManager(QWidget *parent = 0);
    ~PaperManager();

    void settingApply();

protected:
    void closeEvent(QCloseEvent* ev) override;

    void initSetting();
    bool loadSettingFromFile();
    void saveSetting();
    void getCurSetting();
    void setDefault();

    void createTrayIcon();

    void onSettingActionTriggered();
    void onExit();
    void onButtonBoxClicked(QAbstractButton* button);
    void backgroundFileChoose();

private:
    Ui::PaperManager *ui;
    RippleWindow* m_ripplewindow;

    QMenu* m_trayIconMenu;
    QSystemTrayIcon* m_trayIcon;


};

#endif // PAPERMANAGER_H
