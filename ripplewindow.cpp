#include <QOpenGLShaderProgram>
#include <QFile>
#include <QFileInfo>
#include <QWindow>
#include <QDebug>

#include "ripplewindow.h"

//要对此类做修改可能需要一些openGL的基础知识
//此类大量参考引用了一份javascript的代码，我会注释一些我的理解
//参考项目地址https://github.com/sirxemic/jquery.ripples/
//经过查阅资料和推导，应该可以确定update_program使用的算法是基于波动方程的
//你可以在 https://blog.csdn.net/qq_41961619/article/details/114074630 查看详细解释
//着色器中的"precision highp float;\n"应该是从GLSL 1.3才开始支持
//较低的openGL版本可能会在运行时出现着色器链接错误

HHOOK RippleWindow::m_mousehook=NULL;
HWND RippleWindow::m_WinId=NULL;
HWND RippleWindow::m_workerw=NULL;
RippleWindow* RippleWindow::m_instance=nullptr;
int RippleWindow::m_radius=20;
GLfloat RippleWindow::m_strength=0.01;

//openGL所需顶点数据，本程序只需一个正方形的顶点
static GLfloat vertArray[]={
    -1.0,1.0,
    -1.0,-1.0,
    1.0,-1.0,
    1.0,1.0,
    -1.0,1.0,
    1.0,-1.0
};

//本程序共包含drop_program,update_program,render_program三个openGL程序
//他们使用的着色器如下

static const char* globVert=//drop,update使用的顶点着色器
        "attribute vec2 vertex;\n"
        "varying vec2 coord;\n"
        "void main() {\n"
        "	coord = vertex * 0.5 + 0.5;\n"//这里是把(-1.0,1.0)的坐标转换为(0.0,1.0)以便应用于纹理
        "	gl_Position = vec4(vertex, 0.0, 1.0);\n"
        "}\n";

static const char* renderVert=//render使用的顶点着色器
//        "precision highp float;\n"
        "attribute vec2 vertex;\n"
        "varying vec2 ripplesCoord;\n"
        "varying vec2 backgroundCoord;\n"
        "void main() {\n"//水波绘制可以看做我们都是在对纹理进行操作，所以基本都要转换成纹理坐标(0.0,1.0)区间
        "	backgroundCoord=vertex*0.5+0.5;\n"
        "	ripplesCoord=backgroundCoord;\n"
        "	gl_Position = vec4(vertex.x, vertex.y, 0.0, 1.0);\n"
        "}\n";

static const char* renderFrag=//render使用的片段着色器，水波算法的核心部分
//        "precision highp float;\n"
        "uniform sampler2D samplerBackground;\n"//背景图片纹理
        "uniform sampler2D samplerRipples;\n"//帧缓冲中的水波纹理，实际上保存的数据可以看做是水面的高度
        "uniform vec2 delta;\n"//水波精细度参数影响此变量，可以理解成采样点之间的距离
        "uniform float perturbance;\n"//这个参数可以理解成水面的平均高度
        "varying vec2 ripplesCoord;\n"
        "varying vec2 backgroundCoord;\n"
        "void main() {\n"
        "	float height = texture2D(samplerRipples, ripplesCoord).r;\n"//纹理中的r分量保存了水面高度
        "	float heightX = texture2D(samplerRipples, vec2(ripplesCoord.x + delta.x, ripplesCoord.y)).r;\n"//x轴方向相邻采样点的水面高度
        "	float heightY = texture2D(samplerRipples, vec2(ripplesCoord.x, ripplesCoord.y + delta.y)).r;\n"//y轴方向
        "	vec3 dx = vec3(delta.x, heightX - height, 0.0);\n"
        "	vec3 dy = vec3(0.0, heightY - height, delta.y);\n"//以上部分你可以试着从求导数所需参数的角度理解
        "	vec2 offset = -normalize(cross(dy, dx)).xz;\n"//求出水面法线向量，并算出光线在XoY平面内的近似偏移量offset，这里认为光线经过水面后直接折射到法线方向
        "	float specular = pow(max(0.0, dot(offset, normalize(vec2(-0.6, 1.0)))), 4.0);\n"//光的反射，形成水波上的白色高亮
        "	gl_FragColor = texture2D(samplerBackground, backgroundCoord + offset * perturbance) + specular;\n"
        "}\n";//最后将背景图片(x',y')处的色块渲染到(x,y)处以实现折射效果



static const char* updateFrag=//update使用的片段着色器，水波算法核心部分，基于波动方程，不关注波源点及波源数量，以帧为单位同时更新整个水面的高度
//        "precision highp float;\n"
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
        "	info.g += (average - info.r) * 2.0;\n"//g分量存储水面在垂直方向的速度，这里计算出了下一帧的速度
        "	info.g *= damping;\n"//乘以衰减值使水波逐渐减弱
        "	info.r += info.g;\n"//r分量存储水面高度，由公式 路程=速度*时间，这里是算出了下一帧的水面高度
        "	gl_FragColor = info;\n"
        "}\n";

static const char* dropFrag=//drop使用的片段着色器，对帧缓冲中的数据进行操作，用来生成波源
//        "precision highp float;\n"
        "const float PI = 3.141592653589793;\n"
        "uniform sampler2D texture;\n"
        "uniform vec2 center;\n"
        "uniform float radius;\n"
        "uniform float strength;\n"
        "uniform float ratio;\n"//传入的窗口长宽比
        "varying vec2 coord;\n"
        "void main() {\n"
        "	vec4 info = texture2D(texture, coord);\n"
        "	float x=center.x * 0.5 + 0.5 - coord.x;\n"
        "	float y=(center.y * 0.5 + 0.5 - coord.y)*ratio;\n"//纹理坐标空间是正方形，实际上生成了一个椭圆的水波，缩放到屏幕上就会呈现圆形
        "	float drop = max(0.0, 1.0 - length(vec2(x,y)) / radius);\n"//以center为中心，radius为半径修改此范围内的水面高度以使其成为波源
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

    //下面的代码用来把窗口嵌入桌面，网上比较容易查到的方法通常使用SetParent的WinApi
    //但是据我测试SetParent这一句是无效的，即使使用继承自QWindow的类。
    //使用SetParent的代码所实现的在桌面绘制效果，依靠的是showFullscreen这个操作
    //这种操作会使程序打开时先产生一个全屏的窗口，但是你显示桌面时又发现此时在桌面的确渲染了此窗口
    //怎么说呢，至少在较大程度上确实满足了在桌面渲染窗口的需求

    //对于将qt窗口嵌入系统窗口，或是将系统窗口嵌入qt窗口，文档中都介绍了相应的操作，以下的代码产生的行为我认为是比较令人满意的

    HWND desk=FindDesktop::findDesk();//获得窗口句柄
    QWindow* p=QWindow::fromWinId(reinterpret_cast<WId>(desk));//以指定HWND创建窗口的静态函数
    this->setParent(p);//将其设置为我们想嵌入的窗口的父亲
    this->resize(p->size());

}

RippleWindow::~RippleWindow()//qt文档强调了对于你自己创建的openGL相关资源，qt的回收机制无法保证正确进行回收
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

void RippleWindow::swapFrameBuffer()//交换当前所使用的帧缓冲
{
    m_texIndex=1-m_texIndex;//实际上只交换了序号
}

void RippleWindow::initializeGL()//初始化openGL
{

    initializeOpenGLFunctions();

    glClearColor(0.0f,0.0f,0.0f,0.0f);

    m_globVAO.create();//创建VAO,VBO
    m_globVAO.bind();

    m_globVBO.create();
    m_globVBO.bind();

    m_globVBO.allocate(vertArray,sizeof(vertArray));//将顶点数据传入VBO,VAO

    drop_program=new QOpenGLShaderProgram;//绑定drop程序并传入顶点数据
    initProgram(globVert,dropFrag,drop_program);
    drop_program->setAttributeBuffer("vertex",GL_FLOAT,0,2,2*sizeof(GL_FLOAT));
    drop_program->enableAttributeArray("vertex");

    render_program=new QOpenGLShaderProgram;//绑定render
    initProgram(renderVert,renderFrag,render_program);
    render_program->setAttributeBuffer("vertex",GL_FLOAT,0,2,2*sizeof(GL_FLOAT));
    render_program->enableAttributeArray("vertex");


    update_program=new QOpenGLShaderProgram;//绑定update
    initProgram(globVert,updateFrag,update_program);
    update_program->setAttributeBuffer("vertex",GL_FLOAT,0,2,2*sizeof(GL_FLOAT));
    update_program->enableAttributeArray("vertex");

    m_globVAO.release();
    m_globVBO.release();

    if(QFile::exists(m_backgroundImg))//创建背景图片纹理,如果文件不存在则创建一个空的纹理
    {
        m_texture=new QOpenGLTexture(QImage(m_backgroundImg).mirrored());
    }else{
        m_texture=new QOpenGLTexture(QOpenGLTexture::Target2D);
    }
    m_texture->setMagnificationFilter(QOpenGLTexture::Linear);

    unsigned int texture1,texture2,fb1,fb2;//这里创建了两个帧缓冲及他们所使用的纹理附件，注意这是两个额外的纹理，上方的m_texture用来储存真正的背景图片
    //这里的两个纹理是我们通过帧缓冲，利用纹理这种结构保存我们所需的水面高度数据
    glGenFramebuffers(1,&fb1);
    glBindFramebuffer(GL_FRAMEBUFFER,fb1);
    glGenTextures(1, &texture1);

    glBindTexture(GL_TEXTURE_2D, texture1);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);//线性采样理论上是抗锯齿的一种手段，效果好一些，但改成GL_NEAREST似乎也看不出什么区别……
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);//这里的设置可以令水波达到边缘时产生反弹的效果
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F_ARB, this->width(),this->height(), 0, GL_RGBA, GL_FLOAT, NULL);
    //我们想在帧缓冲的纹理附件内保存水面高度，要注意纹理的数据类型要使用GL_FLOAT，而非作为纹理图片时常用的整型颜色值

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture1, 0);

    m_FrameBuffers.push_back(fb1);
    m_Textures.push_back(texture1);

    glGenFramebuffers(1,&fb2);//设置第二个帧缓冲及其纹理，几乎完全同上
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

    //openGL的纹理坐标空间是一个正方形，而我们的窗口很难保证是正方形
    //这里通过向openGL程序中传入窗口长宽比并进行相应的计算进行了坐标转换
    //使得在非正方形的窗口中能够产生圆形的水波
    //参考的js代码使用的是另一种方式，他的方式会使得窗口为非正方形时，水波纹理的某两条边不在窗口范围内
    //也就导致水波无法在窗口边缘反弹，如果你观察够仔细，你可以发现那两条边并非不反弹水波，而是又经过了一段距离才反弹回来。
    m_deltx=m_resolution/this->width();
    m_delty=m_resolution/this->height();
    m_aspectratio=(GLfloat)this->height()/(GLfloat)this->width();//窗口长宽比

    this->setHook();//设置鼠标钩子，获得桌面句柄
    m_WinId=(HWND)this->winId();
    m_workerw=FindDesktop::findWorkerW();

}

void RippleWindow::paintGL()
{
    this->updateFrame();
    this->render();

    update();
}
//与QOpenGLWidget的较大不同在这里，qt文档中说此函数中不推荐使用openGL的功能，如果一定要使用，需要先makeCurrent()获得上下文
//而我编码过程中因为这一块产生了若干问题，最后只好都删掉了，因为桌面窗口基本不需要改变大小，对于本程序倒是没什么影响
void RippleWindow::resizeGL(int width, int height)
{

}

void RippleWindow::initProgram(QString vert,QString frag,QOpenGLShaderProgram* pro){//从文件中加载openGL的着色器程序
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

void RippleWindow::initProgram(const char *vert, const char *frag, QOpenGLShaderProgram *pro)//从字符串加载
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

void RippleWindow::drop(int x,int y,int radius,float strength)//使用drop_program进行绘制，会在framebuffer中添加一个水波
{

    makeCurrent();
    drop_program->bind();

    glBindFramebuffer(GL_FRAMEBUFFER,m_FrameBuffers[1-m_texIndex]);
    m_globVAO.bind();
    float px=(float)(2*x-this->width())/this->width(),py=-(float)(2*y-this->height())/this->height();//把屏幕坐标转换到(-1.0,1.0)的正方形空间内
    float ra=(float)radius/this->width();

    drop_program->setUniformValue("center",px,py);
    drop_program->setUniformValue("radius",ra);
    drop_program->setUniformValue("strength",strength);
    drop_program->setUniformValue("ratio",m_aspectratio);//在此把窗口长宽比传入顶点着色器

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,m_Textures[m_texIndex]);

    glDrawArrays(GL_TRIANGLES,0,6);

    drop_program->release();

    this->swapFrameBuffer();

}

void RippleWindow::updateFrame()//使用update_program绘制，更新framebuffer中的水面高度
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

void RippleWindow::render()//使用render_program进行绘制，计算最终效果并渲染至屏幕
{

    glBindFramebuffer(GL_FRAMEBUFFER,defaultFramebufferObject());//Qt中要使用这种方式重新绑定默认帧缓冲
    render_program->bind();
    render_program->setUniformValue("samplerBackground",0);
    render_program->setUniformValue("samplerRipples",1);
    render_program->setUniformValue("perturbance",(GLfloat)0.04);//这里也是一个会影响水波效果的参数，不过我没提供修改的方法
    render_program->setUniformValue("delta",m_deltx,m_delty);

    m_globVAO.bind();

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D,m_Textures[m_texIndex]);//这里传入水波高度纹理

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,m_texture->textureId());//这里传入背景图片纹理

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
    if(true||GetForegroundWindow()==m_workerw)//如果使用||后面的判断条件，会使仅当桌面获得焦点时才生成新的波源
        //但是实际上占用资源的是水波效果的更新，只要水波效果还在update，性能消耗就差不多，索性就直接true了
        //一种可行的优化是桌面被完全挡住时停止update，以后大概可能也许会改吧
    {
        MOUSEHOOKSTRUCT *mhookstruct = (MOUSEHOOKSTRUCT*)lParam;

        if(wParam==WM_MOUSEMOVE)
        {
            POINT p=mhookstruct->pt;
            if(RippleWindow::m_WinId)//这里我试过使用WinAPI，SendMessage到桌面窗口，可以正确的收到鼠标事件并产生水波
                //但是windows的鼠标消息就被程序拦截掉了，会导致其他窗口无法对鼠标事件作出响应
                //如果你知道如何让qt程序响应windows消息后继续传递该消息，那么SendMessage的方法也是可行的
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
