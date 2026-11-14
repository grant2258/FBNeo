#include <glad/glad.h>
#include <QDebug>
#include <QOpenGLContext>
#include "oglviewport.h"

extern void OGLSetContext(void *ptr);
extern void OGLSetSwapBuffers(void (*f)(void*));
extern void OGLSetMakeCurrent(void (*f)(void*));
extern void OGLSetDoneCurrent(void (*f)(void*));
extern void OGLResize(int w, int h);
extern void OGLSetGetProcAddress(void *(*f)(const char*));

static void *getProcAddress(const char *name)
{
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    return ctx ? reinterpret_cast<void*>(ctx->getProcAddress(name)) : nullptr;
}

static void swapBuffers(void *ptr)
{
    OGLViewport* view = static_cast<OGLViewport*>(ptr);
    view->swapBuffers();
}

static void doneCurrent(void *ptr)
{
    OGLViewport* view = static_cast<OGLViewport*>(ptr);
    view->doneCurrent();
}

static void makeCurrent(void *ptr)
{
    OGLViewport* view = static_cast<OGLViewport*>(ptr);
    view->makeCurrent();
}

OGLViewport::OGLViewport(QWidget *parent) :
    QGLWidget(parent)
{
    QGLFormat format;
    format.setVersion(3, 1);
    format.setProfile(QGLFormat::CoreProfile);
    format.setSwapInterval(0);
    setFormat(format);

    OGLSetContext(this);
    OGLSetSwapBuffers(::swapBuffers);
    OGLSetMakeCurrent(::makeCurrent);
    OGLSetDoneCurrent(::doneCurrent);
    OGLSetGetProcAddress(::getProcAddress);
}

OGLViewport::~OGLViewport()
{

}

void OGLViewport::initializeGL()
{
    doneCurrent();
}

void OGLViewport::paintGL()
{
    doneCurrent();
}

void OGLViewport::resizeGL(int w, int h)
{
    if (context() && context()->isValid()) {
        makeCurrent();
        OGLResize(w, h);
        doneCurrent();
    }
}
