#include <QOpenGLShaderProgram>
#include <QFile>
#include <QFileInfo>
#include <QWindow>
#include <QDebug>

#include "ripplewindow.h"

HHOOK RippleWindow::m_mousehook=NULL;
HWND RippleWindow::m_WinId=NULL;
HWND RippleWindow::m_workerw=NULL;
RippleWindow* RippleWindow::m_instance=nullptr;
int RippleWindow::m_radius=20;
GLfloat RippleWindow::m_strength=0.01;

static GLfloat vertArray[]={
    -1.0,1.0,
    -1.0,-1.0,
    1.0,-1.0,
    1.0,1.0,
    -1.0,1.0,
    1.0,-1.0
};

static const char* globVert=
        "attribute vec2 vertex;\n"
        "varying vec2 coord;\n"
        "void main() {\n"
        "	coord = vertex * 0.5 + 0.5;\n"
        "	gl_Position = vec4(vertex, 0.0, 1.0);\n"
        "}\n";

static const char* renderVert=
        "precision highp float;\n"
        "attribute vec2 vertex;\n"
        "varying vec2 ripplesCoord;\n"
        "varying vec2 backgroundCoord;\n"
        "void main() {\n"
        "	backgroundCoord=vertex*0.5+0.5;\n"
        "	ripplesCoord=backgroundCoord;\n"
        "	gl_Position = vec4(vertex.x, vertex.y, 0.0, 1.0);\n"
        "}\n";

static const char* renderFrag=
        "precision highp float;\n"
        "uniform sampler2D samplerBackground;\n"
        "uniform sampler2D samplerRipples;\n"
        "uniform vec2 delta;\n"
        "uniform float perturbance;\n"
        "varying vec2 ripplesCoord;\n"
        "varying vec2 backgroundCoord;\n"
        "void main() {\n"
        "	float height = texture2D(samplerRipples, ripplesCoord).r;\n"
        "	float heightX = texture2D(samplerRipples, vec2(ripplesCoord.x + delta.x, ripplesCoord.y)).r;\n"
        "	float heightY = texture2D(samplerRipples, vec2(ripplesCoord.x, ripplesCoord.y + delta.y)).r;\n"
        "	vec3 dx = vec3(delta.x, heightX - height, 0.0);\n"
        "	vec3 dy = vec3(0.0, heightY - height, delta.y);\n"
        "	vec2 offset = -normalize(cross(dy, dx)).xz;\n"
        "	float specular = pow(max(0.0, dot(offset, normalize(vec2(-0.6, 1.0)))), 4.0);\n"
        "	gl_FragColor = texture2D(samplerBackground, backgroundCoord + offset * perturbance) + specular;\n"
        "}\n";

static const char* updateFrag=
        "precision highp float;\n"
        "uniform sampler2D texture;\n"
        "uniform vec2 delta;\n"
        "uniform float damping;\n"
        "varying vec2 coord;\n"
        "void main() {\n"
        "	vec4 info = texture2D(texture, coord);\n"
        "	vec2 dx = vec2(delta.x, 0.0);\n"
        "	vec2 dy = vec2(0.0, delta.y);\n"
        "	float average = (\n"
        "		texture2D(texture, coord - dx).r +\n"
        "		texture2D(texture, coord - dy).r +\n"
        "		texture2D(texture, coord + dx).r +\n"
        "		texture2D(texture, coord + dy).r\n"
        "	) * 0.25;\n"
        "	info.g += (average - info.r) * 2.0;\n"
        "	info.g *= damping;\n"
        "	info.r += info.g;\n"
        "	gl_FragColor = info;\n"
        "}\n";

static const char* dropFrag=
        "precision highp float;\n"
        "const float PI = 3.141592653589793;\n"
        "uniform sampler2D texture;\n"
        "uniform vec2 center;\n"
        "uniform float radius;\n"
        "uniform float strength;\n"
        "uniform float ratio;\n"
        "varying vec2 coord;\n"
        "void main() {\n"
        "	vec4 info = texture2D(texture, coord);\n"
        "	float x=center.x * 0.5 + 0.5 - coord.x;\n"
        "	float y=(center.y * 0.5 + 0.5 - coord.y)*ratio;\n"
        "	float drop = max(0.0, 1.0 - length(vec2(x,y)) / radius);\n"
        "	drop = 0.5 - cos(drop * PI) * 0.5;\n"
        "	info.r += drop * strength;\n"
        "	gl_FragColor = info;\n"
        "}\n";

RippleWindow* RippleWindow::getInstance()
{
    if(!m_instance)
    {
        m_instance=new RippleWindow();
    }
    return m_instance;
}

void RippleWindow::destroyRippleWindow()
{
    if(m_instance)
    {
        delete m_instance;
    }
    m_instance=nullptr;
}

RippleWindow::RippleWindow(QWindow* parent)
    :QOpenGLWindow(QOpenGLWindow::PartialUpdateBlend,parent),m_texture(nullptr)
{
    m_texIndex=0;
    m_resolution=2.0;
    m_damping=0.995;
    m_backgroundImg="";

    HWND desk=FindDesktop::findDesk();
    QWindow* p=QWindow::fromWinId(reinterpret_cast<WId>(desk));
    this->setParent(p);
    this->resize(p->size());

}

RippleWindow::~RippleWindow()
{
    this->unHook();
    makeCurrent();
    if(m_texture)
    {
        m_texture->destroy();
        delete m_texture;
    }

    if(m_FrameBuffers.size()>0)
    {
        glDeleteFramebuffers(m_FrameBuffers.size(),m_FrameBuffers.data());
    }
    if(m_Textures.size()>0)
    {
        glDeleteTextures(m_Textures.size(),m_Textures.data());
    }
    m_globVAO.destroy();
    m_globVBO.destroy();
    doneCurrent();

}

void RippleWindow::swapFrameBuffer()
{
    m_texIndex=1-m_texIndex;
}

void RippleWindow::initializeGL()
{

    initializeOpenGLFunctions();

    glClearColor(0.0f,0.0f,0.0f,0.0f);

    m_globVAO.create();
    m_globVAO.bind();

    m_globVBO.create();
    m_globVBO.bind();

    m_globVBO.allocate(vertArray,sizeof(vertArray));

    drop_program=new QOpenGLShaderProgram;
    initProgram(globVert,dropFrag,drop_program);
    drop_program->setAttributeBuffer("vertex",GL_FLOAT,0,2,2*sizeof(GL_FLOAT));
    drop_program->enableAttributeArray("vertex");

    render_program=new QOpenGLShaderProgram;
    initProgram(renderVert,renderFrag,render_program);
    render_program->setAttributeBuffer("vertex",GL_FLOAT,0,2,2*sizeof(GL_FLOAT));
    render_program->enableAttributeArray("vertex");


    update_program=new QOpenGLShaderProgram;
    initProgram(globVert,updateFrag,update_program);
    update_program->setAttributeBuffer("vertex",GL_FLOAT,0,2,2*sizeof(GL_FLOAT));
    update_program->enableAttributeArray("vertex");

    m_globVAO.release();
    m_globVBO.release();

    if(QFile::exists(m_backgroundImg))
    {
        m_texture=new QOpenGLTexture(QImage(m_backgroundImg).mirrored());
    }else{
        m_texture=new QOpenGLTexture(QOpenGLTexture::Target2D);
    }
    m_texture->setMagnificationFilter(QOpenGLTexture::Linear);

    unsigned int texture1,texture2,fb1,fb2;
    glGenFramebuffers(1,&fb1);
    glBindFramebuffer(GL_FRAMEBUFFER,fb1);
    glGenTextures(1, &texture1);

    glBindTexture(GL_TEXTURE_2D, texture1);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F_ARB, this->width(),this->height(), 0, GL_RGBA, GL_FLOAT, NULL);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture1, 0);

    m_FrameBuffers.push_back(fb1);
    m_Textures.push_back(texture1);

    glGenFramebuffers(1,&fb2);
    glBindFramebuffer(GL_FRAMEBUFFER,fb2);
    glGenTextures(1, &texture2);

    glBindTexture(GL_TEXTURE_2D, texture2);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F_ARB, this->width(),this->height(), 0, GL_RGBA, GL_FLOAT, NULL);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture2, 0);

    m_FrameBuffers.push_back(fb2);
    m_Textures.push_back(texture2);


    m_deltx=m_resolution/this->width();
    m_delty=m_resolution/this->height();
    m_aspectratio=(GLfloat)this->height()/(GLfloat)this->width();

    this->setHook();
    m_WinId=(HWND)this->winId();
    m_workerw=FindDesktop::findWorkerW();

}

void RippleWindow::paintGL()
{
    this->updateFrame();
    this->render();

    update();
}

void RippleWindow::resizeGL(int width, int height)
{

}

void RippleWindow::initProgram(QString vert,QString frag,QOpenGLShaderProgram* pro){
    if(!pro->addShaderFromSourceFile(QOpenGLShader::Vertex,vert))
    {
        qDebug()<< vert<<(pro->log());
        return;
    }
    if(!pro->addShaderFromSourceFile(QOpenGLShader::Fragment,frag))
    {
        qDebug()<< vert<<(pro->log());
        return;
    }
    if(!pro->link())
    {
        qDebug()<<vert<< (pro->log());
        return;
    }
}

void RippleWindow::initProgram(const char *vert, const char *frag, QOpenGLShaderProgram *pro)
{
    if(!pro->addShaderFromSourceCode(QOpenGLShader::Vertex,vert))
    {
        qDebug()<< (pro->log());
        return;
    }
    if(!pro->addShaderFromSourceCode(QOpenGLShader::Fragment,frag))
    {
        qDebug()<< (pro->log());
        return;
    }
    if(!pro->link())
    {
        qDebug()<< (pro->log());
        return;
    }
}

void RippleWindow::mouseMoveEvent(QMouseEvent* ev)
{
    this->drop(ev->x(),ev->y(),m_radius,m_strength);
}

void RippleWindow::mousePressEvent(QMouseEvent *ev)
{
    this->drop(ev->x(),ev->y(),1.5*m_radius,14*m_strength);
}

void RippleWindow::drop(int x,int y,int radius,float strength)
{

    makeCurrent();
    drop_program->bind();

    glBindFramebuffer(GL_FRAMEBUFFER,m_FrameBuffers[1-m_texIndex]);
    m_globVAO.bind();
    float px=(float)(2*x-this->width())/this->width(),py=-(float)(2*y-this->height())/this->height();
    float ra=(float)radius/this->width();

    drop_program->setUniformValue("center",px,py);
    drop_program->setUniformValue("radius",ra);
    drop_program->setUniformValue("strength",strength);
    drop_program->setUniformValue("ratio",m_aspectratio);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,m_Textures[m_texIndex]);

    glDrawArrays(GL_TRIANGLES,0,6);

    drop_program->release();

    this->swapFrameBuffer();

}

void RippleWindow::updateFrame()
{
    makeCurrent();

    glBindFramebuffer(GL_FRAMEBUFFER,m_FrameBuffers[1-m_texIndex]);
    m_globVAO.bind();
    update_program->bind();

    update_program->setUniformValue("delta",m_deltx,m_delty);
    update_program->setUniformValue("damping",m_damping);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,m_Textures[m_texIndex]);

    glDrawArrays(GL_TRIANGLES,0,6);
    update_program->release();

    glBindFramebuffer(GL_FRAMEBUFFER,defaultFramebufferObject());
    this->swapFrameBuffer();
}

void RippleWindow::render()
{

    glBindFramebuffer(GL_FRAMEBUFFER,defaultFramebufferObject());
    render_program->bind();
    render_program->setUniformValue("samplerBackground",0);
    render_program->setUniformValue("samplerRipples",1);
    render_program->setUniformValue("perturbance",(GLfloat)0.04);
    render_program->setUniformValue("delta",m_deltx,m_delty);

    m_globVAO.bind();

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D,m_Textures[m_texIndex]);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,m_texture->textureId());

    glDrawArrays(GL_TRIANGLES,0,6);
    render_program->release();

}

void RippleWindow::setRadius(int radius)
{
    m_radius=radius;
}

void RippleWindow::setStrength(GLfloat strength)
{
    if(strength>0&&strength<1)
    {
        m_strength=strength;
    }
}

void RippleWindow::setResolution(GLfloat resolution)
{
    if(resolution>0)
    {
        m_resolution=resolution;
        m_deltx=m_resolution/this->width();
        m_delty=m_resolution/this->height();
    }
}

void RippleWindow::setDamping(GLfloat damping)
{
    if(damping>=1||damping<0)
    {
        return;
    }
    m_damping=damping;
}

void RippleWindow::setBackgroundImage(QString filename)
{
    makeCurrent();
    if(QFile::exists(filename))
    {
        QFileInfo fi(filename);
        m_backgroundImg=fi.absoluteFilePath();
        if(m_texture)
        {
            m_texture->destroy();
            delete m_texture;
            m_texture=new QOpenGLTexture(QImage(filename).mirrored());

        }

    }
}

int RippleWindow::getRadius()
{
    return m_radius;
}

GLfloat RippleWindow::getStrength()
{
    return m_strength;
}

GLfloat RippleWindow::getResolution()
{
    return m_resolution;
}

GLfloat RippleWindow::getDamping()
{
    return m_damping;
}

QString RippleWindow::getBackground()
{
    return m_backgroundImg;
}

void RippleWindow::accEvent(QEvent *ev)
{
    this->event(ev);
}

LRESULT CALLBACK RippleWindow::mouseProc(int Code, WPARAM wParam, LPARAM lParam)
{
    if(Code<0)
    {
        return CallNextHookEx(m_mousehook,Code,wParam,lParam);
    }
    if(true||GetForegroundWindow()==m_workerw)
    {
        MOUSEHOOKSTRUCT *mhookstruct = (MOUSEHOOKSTRUCT*)lParam;

        if(wParam==WM_MOUSEMOVE)
        {
            POINT p=mhookstruct->pt;
            if(RippleWindow::m_WinId)
            {
                m_instance->drop(p.x,p.y,RippleWindow::m_radius,RippleWindow::m_strength);
            }
        }
        if(wParam==WM_LBUTTONDOWN)
        {
            POINT p=mhookstruct->pt;
            if(RippleWindow::m_WinId)
            {
                m_instance->drop(p.x,p.y,1.5*RippleWindow::m_radius,14*RippleWindow::m_strength);
            }
        }
    }
    return CallNextHookEx(m_mousehook,Code,wParam,lParam);

}

void RippleWindow::setHook()
{
    HINSTANCE hInstance = GetModuleHandle(NULL);
    m_mousehook =  SetWindowsHookEx(WH_MOUSE_LL, mouseProc, hInstance, 0);
    if(m_mousehook==NULL)
    {
        qDebug()<<"set hook failed";
    }

}

void RippleWindow::unHook()
{
    if(m_mousehook)
    {
        UnhookWindowsHookEx(m_mousehook);
    }
}
