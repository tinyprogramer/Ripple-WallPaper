#ifndef RIPPLEWINDOW_H
#define RIPPLEWINDOW_H

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

class RippleWindow : public QOpenGLWindow,protected QOpenGLFunctions
{

    Q_OBJECT

public:

    static RippleWindow* getInstance();
    static void destroyRippleWindow();
    ~RippleWindow();
    void printcnt();
    void accEvent(QEvent* ev);
    void drop(int x,int y,int radius,float strength);

    void setRadius(int radius);
    void setStrength(GLfloat strength);
    void setResolution(GLfloat resolution);
    void setDamping(GLfloat damping);
    void setBackgroundImage(QString filename);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int width, int height) override;
    virtual void mouseMoveEvent(QMouseEvent* ev);
    virtual void mousePressEvent(QMouseEvent* ev);
    void swapFrameBuffer();
    void render();
    void updateFrame();
    void initProgram(QString vert,QString frag,QOpenGLShaderProgram* pro);
    void initProgram(const char* vert,const char* frag,QOpenGLShaderProgram* pro);
    //bool eventFilter(QObject *watched, QEvent *event);

    void setHook();
    void unHook();
    static LRESULT CALLBACK mouseProc(int Code, WPARAM wParam, LPARAM lParam);


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
