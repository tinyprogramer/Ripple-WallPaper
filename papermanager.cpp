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
    this->setAttribute(Qt::WA_DeleteOnClose);

    this->initSetting();

    QObject::connect(ui->buttonBox,&QDialogButtonBox::clicked,this,&PaperManager::onButtonBoxClicked);
    QObject::connect(ui->buttonBox,&QDialogButtonBox::accepted,[this](){
        this->settingApply();
        this->close();
    });
    QObject::connect(ui->FileChooseButton,&QPushButton::clicked,this,&PaperManager::backgroundFileChoose);

    this->createTrayIcon();
    m_trayIcon->setIcon(QIcon("E:/qtproject/systray/images/bad.png"));
    m_trayIcon->show();
    m_ripplewindow=RippleWindow::getInstance();

    if(!this->loadSettingFromFile())
    {
        this->setDefault();
    }
    this->settingApply();

    m_ripplewindow->show();

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

    QJsonDocument loadDoc(QJsonDocument::fromBinaryData(saveData));
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
    fdp->setOption(QFileDialog::DontResolveSymlinks);
    fdp->setNameFilter("Images (*.jpg *.png *.bmp *.jpeg)");
    if(m_ripplewindow)
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
    ev->ignore();
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
    this->getCurSetting();
    this->show();
}

void PaperManager::onExit()
{
    m_trayIcon->hide();
    this->hide();
    if(m_ripplewindow)
    {
        m_ripplewindow->destroyRippleWindow();
    }

    this->reject();
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
