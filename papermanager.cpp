#include <QAction>
#include <QString>
#include <QMessageBox>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFileDialog>
#include <QPointer>

#include "papermanager.h"
#include "ui_papermanager.h"

PaperManager::PaperManager(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PaperManager)
{
    ui->setupUi(this);
    this->setAttribute(Qt::WA_DeleteOnClose);//设置关闭后删除
    this->setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);//去掉右上角问号按钮

    this->initSetting();

    //把buttonBox中的文字改为中文
    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(QString::fromWCharArray(L"确定"));
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(QString::fromWCharArray(L"取消"));
    ui->buttonBox->button(QDialogButtonBox::Apply)->setText(QString::fromWCharArray(L"应用"));
    ui->buttonBox->button(QDialogButtonBox::RestoreDefaults)->setText(QString::fromWCharArray(L"恢复默认"));
    QObject::connect(ui->buttonBox,&QDialogButtonBox::clicked,this,&PaperManager::onButtonBoxClicked);
    QObject::connect(ui->buttonBox,&QDialogButtonBox::accepted,[this](){
        this->settingApply();
        this->close();
    });
    QObject::connect(ui->FileChooseButton,&QPushButton::clicked,this,&PaperManager::backgroundFileChoose);

    this->createTrayIcon();
    m_trayIcon->setIcon(QIcon(":/icon/settings.png"));
    m_trayIcon->show();//设置托盘图标并显示

    m_ripplewindow=RippleWindow::getInstance();//获取单例实例

    if(!this->loadSettingFromFile())//从json文件获取设置，如失败则使用默认设置
    {
        this->setDefault();
    }
    this->settingApply();

    m_ripplewindow->show();
    m_trayIcon->showMessage(QString::fromWCharArray(L"壁纸"),QString::fromWCharArray(L"壁纸已启动，可在此更改设置"));
    //壁纸窗口已生成，托盘弹出消息提示

}

PaperManager::~PaperManager()
{
    delete ui;
}

void PaperManager::initSetting()
{
    ui->RadiusSlider->setMaximum(100);
    ui->RadiusSlider->setMinimum(5);

    ui->StrengthSlider->setMaximum(100);
    ui->StrengthSlider->setMinimum(1);

    ui->ResolutionSlider->setMaximum(16);
    ui->ResolutionSlider->setMinimum(1);

    ui->DampingSlider->setMaximum(1000);
    ui->DampingSlider->setMinimum(0);
}

bool PaperManager::loadSettingFromFile()
{
    QFile loadFile(QStringLiteral("save.dat"));

    if (!loadFile.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray saveData = loadFile.readAll();

    QJsonDocument loadDoc(QJsonDocument::fromBinaryData(saveData));//这里使用二进制的方式读取和保存json文件
    QJsonObject data=loadDoc.object();

    if(!(data.contains("Radius")&&data["Radius"].isDouble()&&
         data.contains("Strength")&&data["Strength"].isDouble()&&
         data.contains("Resolution")&&data["Resolution"].isDouble()&&
         data.contains("Damping")&&data["Damping"].isDouble()&&
         data.contains("Background")&&data["Background"].isString()))
    {
        return false;
    }

    ui->RadiusSlider->setValue(data["Radius"].toInt());
    ui->StrengthSlider->setValue(data["Strength"].toDouble());
    ui->ResolutionSlider->setValue(data["Resolution"].toDouble());
    ui->DampingSlider->setValue(data["Damping"].toDouble());
    ui->lineEdit->setText(data["Background"].toString());

    return true;
}

void PaperManager::saveSetting()
{
    QFile saveFile(QStringLiteral("save.dat"));

    if (!saveFile.open(QIODevice::WriteOnly)) {
        return;
    }

    QJsonObject saveObject;

    this->getCurSetting();

    saveObject["Radius"]=ui->RadiusSlider->value();
    saveObject["Strength"]=ui->StrengthSlider->value();
    saveObject["Resolution"]=ui->ResolutionSlider->value();
    saveObject["Damping"]=ui->DampingSlider->value();
    saveObject["Background"]=ui->lineEdit->text();

    QJsonDocument saveDoc(saveObject);
    saveFile.write(saveDoc.toBinaryData());

}
//有关参数设置的几个函数并没有很好的抽象出逻辑，如果你想修改参数区间，可能要同时修改
//initSetting(),getCurSetting(),settingApply()函数来使参数能够正确的转换
void PaperManager::getCurSetting()
{
    if(!m_ripplewindow)
    {
        return;
    }
    ui->RadiusSlider->setValue(m_ripplewindow->getRadius());
    ui->StrengthSlider->setValue(m_ripplewindow->getStrength()*1000);
    ui->ResolutionSlider->setValue(17-m_ripplewindow->getResolution()*2);
    ui->DampingSlider->setValue((m_ripplewindow->getDamping()-0.899)*10000);
    ui->lineEdit->setText(m_ripplewindow->getBackground());
}

void PaperManager::setDefault()
{
    ui->RadiusSlider->setValue(30);
    ui->StrengthSlider->setValue(10);
    ui->ResolutionSlider->setValue(11);
    ui->DampingSlider->setValue(960);
    this->settingApply();
}

void PaperManager::settingApply()
{
    if(!m_ripplewindow)
    {
        return;
    }
    m_ripplewindow->setRadius(ui->RadiusSlider->value());
    m_ripplewindow->setStrength((float)ui->StrengthSlider->value()/1000.0);
    m_ripplewindow->setResolution((float)(17-ui->ResolutionSlider->value())/2.0);
    m_ripplewindow->setDamping((float)ui->DampingSlider->value()/10000.0+0.899);
    m_ripplewindow->setBackgroundImage(ui->lineEdit->text());

    this->saveSetting();
}

void PaperManager::backgroundFileChoose()
{
    QPointer<QFileDialog> fdp=new QFileDialog(this);
    QObject::connect(fdp,&QFileDialog::fileSelected,[this](const QString& filename){
        ui->lineEdit->setText(filename);
    });
    fdp->setAttribute(Qt::WA_DeleteOnClose);
    fdp->setWindowModality(Qt::ApplicationModal);
    fdp->setOption(QFileDialog::DontResolveSymlinks);//不解决符号链接，否则选择某些文件会导致程序卡死
    fdp->setNameFilter("Images (*.jpg *.png *.bmp *.jpeg)");//加了过滤器依然有一些链接文件会出现在窗口中
    if(m_ripplewindow)//如果此时在使用有效的背景图片，就尝试将初始目录设置为此图片所在的目录
    {
        const QString& bak=m_ripplewindow->getBackground();
        if(QFile::exists(bak))
        {
            QFileInfo finfo(bak);
            fdp->setDirectory(finfo.path());
        }
    }
    fdp->show();
}

void PaperManager::closeEvent(QCloseEvent *ev)
{
    this->hide();
    ev->ignore();//忽略关闭事件改为隐藏窗口
}

void PaperManager::createTrayIcon()
{
    m_trayIconMenu = new QMenu(this);
    QAction* settingAction=new QAction(QString::fromWCharArray(L"设置"), this);
    QObject::connect(settingAction,&QAction::triggered,this,&PaperManager::onSettingActionTriggered);
    m_trayIconMenu->addAction(settingAction);
    m_trayIconMenu->addSeparator();
    QAction* quitAction = new QAction(QString::fromWCharArray(L"退出"), this);
    QObject::connect(quitAction, &QAction::triggered, this,&PaperManager::onExit);
    m_trayIconMenu->addAction(quitAction);

    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setContextMenu(m_trayIconMenu);

}

void PaperManager::onSettingActionTriggered()
{
    this->getCurSetting();//显示窗口前将滑块置于正确位置，用户上次可能点了取消
    this->show();//这样每次打开设置窗口时，滑块都能正确的表示当前的参数设置
}

void PaperManager::onExit()
{
    //尽管需要回收的项目不多，直接调用qApp->quit()可以退出程序
    //但是还是尝试手动回收一下，直接quit()并不会调用此类的析构函数，错误的回收可能会导致程序无法正确退出
    m_trayIcon->hide();//这里不隐藏的话，程序关闭后托盘图标会一直存在直到鼠标移向它，影响体验
    this->hide();
    if(m_ripplewindow)
    {
        m_ripplewindow->destroyRippleWindow();
    }

    this->reject();//reject会令窗口关闭而不是隐藏
}

void PaperManager::onButtonBoxClicked(QAbstractButton* button)
{
    if(ui->buttonBox->buttonRole(button)==QDialogButtonBox::ApplyRole)
    {
        this->settingApply();
    }else if(ui->buttonBox->buttonRole(button)==QDialogButtonBox::RejectRole)
    {
        this->hide();
    }else if(ui->buttonBox->buttonRole(button)==QDialogButtonBox::ResetRole)
    {
        if(QMessageBox::question(this,QString::fromWCharArray(L"提示"),
                                    QString::fromWCharArray(L"确定恢复为默认值吗？"),
                                 QMessageBox::Ok,QMessageBox::Cancel)==QMessageBox::Ok){
        this->setDefault();
        }
    }
}
