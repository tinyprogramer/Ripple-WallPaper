#ifndef RIPPLEWINDOW_H
#define RIPPLEWINDOW_H

#pragma execution_character_set("utf-8")

#include <QOpenGLWindow>
#include <QOpenGLFunctions>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QOpenGLTexture>
#include <QMouseEvent>
#include <QOpenGLShaderProgram>
#include <vector>
#include <QString>

#include "finddesktop.h"
//此类用来编写openGL窗口，实现水波的绘制
//QWidget和QWindow似乎不是一个层面的东西，二者也无法简单的互相转化
//因为我们要跟windows窗口建立联系,使用继承自QWindow的类看起来能够获得更令人满意的行为
//得益于Qt对QOpenGLWidget和QOpenGLWindow的封装比较相似，我可以比较容易的把之前的代码改为window而使用
class RippleWindow : public QOpenGLWindow,protected QOpenGLFunctions
{

    Q_OBJECT

public:

    static RippleWindow* getInstance();//在捕获并传递鼠标事件的过程中遇到了点麻烦
    //考虑了一下还是单例模式比较适合解决此问题(也是因为对windows机制不够了解)
    //单例并非必要，只是综合考虑了最终效果和修改难度之后做的折中
    static void destroyRippleWindow();
    ~RippleWindow();

    void accEvent(QEvent* ev);//此程序中无用
    void drop(int x,int y,int radius,float strength);//添加波源点

    void setRadius(int radius);//设置水波半径
    void setStrength(GLfloat strength);//设置水波强度
    void setResolution(GLfloat resolution);//设置水波精细度
    void setDamping(GLfloat damping);//设置水波衰减速度
    void setBackgroundImage(QString filename);//设置背景图片

    int getRadius();//获取当前设置的参数
    GLfloat getStrength();
    GLfloat getResolution();
    GLfloat getDamping();
    QString getBackground();

protected:
    void initializeGL() override;//openGL相关
    void paintGL() override;
    void resizeGL(int width, int height) override;

    virtual void mouseMoveEvent(QMouseEvent* ev);//此程序最终没有使用事件的方式
    virtual void mousePressEvent(QMouseEvent* ev);//但并不是无法使用

    void swapFrameBuffer();//openGL相关
    void render();
    void updateFrame();
    void initProgram(QString vert,QString frag,QOpenGLShaderProgram* pro);
    void initProgram(const char* vert,const char* frag,QOpenGLShaderProgram* pro);

    //程序体量比较小，水平也有限，我就不费心思抽象结构造轮子了，所以一些WinAPI的操作也塞到这里了
    void setHook();//设置鼠标钩子
    void unHook();//卸载钩子
    static LRESULT CALLBACK mouseProc(int Code, WPARAM wParam, LPARAM lParam);//鼠标钩子回调函数


private:
    RippleWindow(QWindow* parent=0);
    QOpenGLShaderProgram *drop_program,*render_program,*update_program;
    QOpenGLVertexArrayObject m_globVAO;

    std::vector<unsigned int> m_FrameBuffers,m_Textures;
    QOpenGLBuffer m_globVBO;
    QOpenGLTexture* m_texture;
    int m_texIndex;
    GLfloat m_deltx,m_delty,m_resolution,m_aspectratio,m_damping;
    QString m_backgroundImg;

    static HHOOK m_mousehook;
    static HWND m_WinId,m_workerw;

    static int m_radius;
    static GLfloat m_strength;

    static RippleWindow* m_instance;

};

#endif // RIPPLEWINDOW_H
