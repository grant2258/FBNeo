// OpenGL 3.3 Core Profile renderer
#include <glad/glad.h>
#include <iostream>
#include "burner.h"

#include "shader.h"

static int videoLenght = 0;
static int videoPitch = 0;
static int videoBpp = 0;
static int videoClipWidth = 0;
static int videoClipHeight = 0;
static int videoWidth = 0;
static int videoHeight = 0;
static unsigned char *videoMemory = nullptr;
static bool rotateVideo = false;

static bool isGladInitialized = false;
static void *(*oglGetProcAddress)(const char*) = nullptr;

void OGLSetGetProcAddress(void *(*f)(const char*))
{
    oglGetProcAddress = f;
}

static void ensureGladLoaded()
{
    if (!isGladInitialized && oglGetProcAddress) {
        if (gladLoadGLLoader((GLADloadproc)oglGetProcAddress)) {
            isGladInitialized = true;
        }
    }
}
static GLuint vao, texture;

static void (*oglSwapBuffers)(void*) = nullptr;
static void (*oglMakeCurrent)(void*) = nullptr;
static void (*oglDoneCurrent)(void*) = nullptr;
static void *oglContext = nullptr;
static Shader *stockShader = nullptr;


static const string defaultVertexShader = R"(
        #version 140
        #extension GL_ARB_explicit_attrib_location : enable

        layout (location = 0) in vec2 position;
        layout (location = 1) in vec2 uvCoord;

        out vec2 texCoord;

        void main() {
            gl_Position = vec4(position, 0, 1.0f);
            texCoord = uvCoord;
        }
)";

static const string defaultFragmentShader = R"(
        #version 140

        out vec4 color;
        in vec2 texCoord;

        uniform sampler2D tex;
        uniform float width;
        uniform float height;

        void main() {
            color = texture(tex, texCoord);
        }
)";


void OGLSetContext(void *ptr)
{
    oglContext = ptr;
}

void OGLSetSwapBuffers(void (*f)(void*))
{
    oglSwapBuffers = f;
}

void OGLSetMakeCurrent(void (*f)(void*))
{
    oglMakeCurrent = f;
}

void OGLSetDoneCurrent(void (*f)(void*))
{
    oglDoneCurrent = f;
}

static int winWidth = 640;
static int winHeight = 480;

static void applyAspectViewport(int w, int h)
{
    if (w <= 0 || h <= 0)
        return;

    int targetW = w;
    int targetH = h;
    INT32 aspX = 4, aspY = 3;

    if (bDrvOkay) {
        BurnDrvGetAspect(&aspX, &aspY);
        if (aspX <= 0 || aspY <= 0) {
            aspX = rotateVideo ? 3 : 4;
            aspY = rotateVideo ? 4 : 3;
        }
    } else if (rotateVideo) {
        aspX = 3;
        aspY = 4;
    }

    if (w * aspY > h * aspX) {
        targetW = (h * aspX) / aspY;
        targetH = h;
    } else {
        targetW = w;
        targetH = (w * aspY) / aspX;
    }

    int vpX = (w - targetW) / 2;
    int vpY = (h - targetH) / 2;
    glViewport(vpX, vpY, targetW, targetH);
}

void OGLResize(int w, int h)
{
    winWidth = w;
    winHeight = h;
    ensureGladLoaded();
    if (!isGladInitialized)
        return;
    applyAspectViewport(w, h);
}

static INT32 oglInit()
{
    if (oglMakeCurrent)
        oglMakeCurrent(oglContext);

    ensureGladLoaded();

    stockShader = new Shader(defaultVertexShader, defaultFragmentShader, true);

    unsigned flags = BurnDrvGetFlags();

    if (flags & BDF_ORIENTATION_VERTICAL) {
        BurnDrvGetVisibleSize(&videoClipHeight, &videoClipWidth);
        BurnDrvGetFullSize(&videoWidth, &videoHeight);
        rotateVideo = true;
    } else {
        BurnDrvGetVisibleSize(&videoClipWidth, &videoClipHeight);
        BurnDrvGetFullSize(&videoWidth, &videoHeight);
        rotateVideo = false;
    }

    videoBpp = nBurnBpp = 2;
    videoPitch = videoWidth * videoBpp;
    videoLenght = videoHeight * videoPitch;
    videoMemory = new unsigned char[videoLenght];
    SetBurnHighCol(16);

    glGenVertexArrays(1, &vao);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565, videoWidth, videoHeight, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (oglDoneCurrent)
        oglDoneCurrent(oglContext);

    return 0;
}

static INT32 oglExit()
{
    delete stockShader;
    glDeleteTextures(1, &texture);
    glDeleteVertexArrays(1, &vao);
    delete [] videoMemory;
    return 0;
}

static INT32 oglFrame(bool bRedraw)
{
    nBurnBpp = 2;

    nBurnPitch = videoPitch;
    pBurnDraw = videoMemory;

    if (bDrvOkay) {
        pBurnSoundOut = nAudNextSound;
        nBurnSoundLen = nAudSegLen;
		VidFrameCallback(bRedraw);
    }

    pBurnDraw = NULL;
    nBurnPitch = 0;
    return 0;
}

static INT32 oglPaint(INT32 bValidate)
{
    if (oglMakeCurrent)
        oglMakeCurrent(oglContext);

    glViewport(0, 0, winWidth, winHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    applyAspectViewport(winWidth, winHeight);

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, videoWidth, videoHeight, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, videoMemory);
    glBindTexture(GL_TEXTURE_2D, 0);

    GLfloat vtx[16];
    if (rotateVideo) {
        bool flipped = (BurnDrvGetFlags() & BDF_ORIENTATION_FLIPPED) != 0;
        if (!flipped) {
            // 90 degrees CCW (standard vertical arcade monitor)
            GLfloat rot[] = {
                -1.0f,  1.0f,   1.0f, 0.0f,
                -1.0f, -1.0f,   0.0f, 0.0f,
                 1.0f,  1.0f,   1.0f, 1.0f,
                 1.0f, -1.0f,   0.0f, 1.0f
            };
            memcpy(vtx, rot, sizeof(vtx));
        } else {
            // 90 degrees CW (flipped vertical)
            GLfloat rot[] = {
                -1.0f,  1.0f,   0.0f, 1.0f,
                -1.0f, -1.0f,   1.0f, 1.0f,
                 1.0f,  1.0f,   0.0f, 0.0f,
                 1.0f, -1.0f,   1.0f, 0.0f
            };
            memcpy(vtx, rot, sizeof(vtx));
        }
    } else {
        bool flipped = (BurnDrvGetFlags() & BDF_ORIENTATION_FLIPPED) != 0;
        if (!flipped) {
            GLfloat norm[] = {
                -1.0f,  1.0f,   0.0f, 0.0f,
                -1.0f, -1.0f,   0.0f, 1.0f,
                 1.0f,  1.0f,   1.0f, 0.0f,
                 1.0f, -1.0f,   1.0f, 1.0f
            };
            memcpy(vtx, norm, sizeof(vtx));
        } else {
            // 180 degrees flip (flipped horizontal games like RoboCop)
            GLfloat flip[] = {
                -1.0f,  1.0f,   1.0f, 1.0f,
                -1.0f, -1.0f,   1.0f, 0.0f,
                 1.0f,  1.0f,   0.0f, 1.0f,
                 1.0f, -1.0f,   0.0f, 0.0f
            };
            memcpy(vtx, flip, sizeof(vtx));
        }
    }
    glBindVertexArray(vao);

    GLuint vbo;
    glGenBuffers(1, &vbo);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vtx), vtx, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*) (2 * sizeof(GLfloat)));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    stockShader->use();
    stockShader->setUniform("width", static_cast<float>(videoWidth));
    stockShader->setUniform("height", static_cast<float>(videoHeight));
    glBindTexture(GL_TEXTURE_2D, texture);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glDeleteBuffers(1, &vbo);
    glBindVertexArray(0);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
        std::cerr << "GL error in oglPaint: 0x" << std::hex << err << std::endl;

    if (oglSwapBuffers)
        oglSwapBuffers(oglContext);

    if (oglDoneCurrent)
        oglDoneCurrent(oglContext);
    return 0;
}

static INT32 oglImageSize(RECT* pRect, INT32 nGameWidth, INT32 nGameHeight)
{
    return 0;
}

// Get plugin info
static INT32 oglGetPluginSettings(InterfaceInfo* pInfo)
{
    return 0;
}

struct VidOut VidOutOGL = { oglInit, oglExit, oglFrame, oglPaint,
                          oglImageSize, oglGetPluginSettings,
                          ("ogl-video") };
