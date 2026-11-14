#include <QtWidgets>
#include <QtMultimedia>
#include "burner.h"
#include "qaudiointerface.h"

QAudioInterface *qAudio = nullptr;
int (*QtSoundGetNextSound)(int);

static int nQtAudioFps = 0;

static int QtSoundGetNextSoundFiller(int)
{
    if (nAudNextSound == nullptr)
        return 1;
    memset(nAudNextSound, 0, nAudSegLen * 4);
    return 0;
}

static int QtSoundBlankSound()
{
    if (nAudNextSound != nullptr)
        AudWriteSilence();
    return 0;
}

static QElapsedTimer s_frameTimer;
static qint64 s_nextFrameNs = 0;

static int QtSoundCheck()
{
    if (!qAudio || !qAudio->isReady()) {
        QThread::usleep(1000);
        s_nextFrameNs = 0;
        return 0;
    }

    int fps = (nQtAudioFps > 0) ? nQtAudioFps : 6000;
    qint64 framePeriodNs = 100000000000LL / fps;

    if (!s_frameTimer.isValid() || s_nextFrameNs == 0) {
        s_frameTimer.start();
        s_nextFrameNs = s_frameTimer.nsecsElapsed();
    }

    qint64 nowNs = s_frameTimer.nsecsElapsed();
    qint64 remainingNs = s_nextFrameNs - nowNs;

    // Sleep coarse chunks until within 1.5 ms of target frame boundary, then yield cleanly
    if (remainingNs > 1500000LL) {
        QThread::usleep((remainingNs - 1000000LL) / 1000LL);
        return 0;
    } else if (remainingNs > 0) {
        QThread::yieldCurrentThread();
        return 0;
    }

    int segBytes = nAudSegLen << 2;
    if (qAudio->bytesFree() < segBytes) {
        // Audio buffer full: nudge target slightly so clock stays locked to DAC rate
        s_nextFrameNs = nowNs + 500000LL;
        QThread::usleep(500);
        return 0;
    }

    // Advance target timestamp by exact frame period; clamp drift if stalled > 2 frames
    s_nextFrameNs += framePeriodNs;
    if (nowNs - s_nextFrameNs > framePeriodNs * 2) {
        s_nextFrameNs = nowNs + framePeriodNs;
    }

    QtSoundGetNextSound(1);
    qAudio->writeAudio((const char *)nAudNextSound, segBytes);
    return 0;
}

static int QtSoundExit()
{
    return 0;
}

static int QtSoundSetCallback(int (*pCallback)(int))
{
    if (pCallback == nullptr)
        QtSoundGetNextSound = QtSoundGetNextSoundFiller;
    else
        QtSoundGetNextSound = pCallback;
    return 0;
}

static int QtSoundInit()
{
    nQtAudioFps = (nBurnFPS > 0) ? nBurnFPS : nAppVirtualFps;
    nAudSegLen = (nAudSampleRate[0] * 100 + (nQtAudioFps / 2)) / nQtAudioFps;
    nAudAllocSegLen = nAudSegLen * 4;

    if (nAudNextSound != nullptr) {
        delete[] nAudNextSound;
        nAudNextSound = nullptr;
    }
    nAudNextSound = new short[nAudAllocSegLen / sizeof(short)];

    QtSoundSetCallback(nullptr);
    QtSoundGetNextSoundFiller(0);

    pBurnSoundOut = nAudNextSound;
    nBurnSoundRate = nAudSampleRate[0];
    nBurnSoundLen = nAudAllocSegLen;

    qAudio = QAudioInterface::get();
    QAudioFormat format;
    format.setChannelCount(2);
    format.setSampleRate(nAudSampleRate[0]);
    format.setSampleType(QAudioFormat::SignedInt);
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleSize(16);
    qAudio->setFormat(format);

    QMetaObject::invokeMethod(qAudio, "init", Qt::BlockingQueuedConnection);

    bAudOkay = 1;
    return 0;
}

static int QtSoundPlay()
{
    if (qAudio)
        QMetaObject::invokeMethod(qAudio, "play", Qt::BlockingQueuedConnection);
    bAudPlaying = 1;
    return 0;
}

static int QtSoundStop()
{
    if (qAudio)
        QMetaObject::invokeMethod(qAudio, "stop", Qt::BlockingQueuedConnection);
    bAudPlaying = 0;
    return 0;
}

static int QtSoundSetVolume()
{
    return 1;
}

static int QtSoundGetSettings(InterfaceInfo *info)
{
    Q_UNUSED(info);
    return 0;
}

struct AudOut AudOutQtSound = { QtSoundBlankSound, QtSoundCheck,
                                QtSoundInit, QtSoundSetCallback, QtSoundPlay,
                                QtSoundStop, QtSoundExit, QtSoundSetVolume,
                                QtSoundGetSettings, ("qt-audio") };

QAudioInterface *QAudioInterface::m_onlyInstance = nullptr;

QAudioInterface::QAudioInterface(QObject *parent) :
    QThread()
{
    m_audioOutput = nullptr;
    m_buffer = nullptr;
    moveToThread(this);
    start();
}

QAudioInterface::~QAudioInterface()
{
    if (m_audioOutput != nullptr) {
        delete m_audioOutput;
        m_audioOutput = nullptr;
    }
}

void QAudioInterface::run()
{
    exec();
}

QAudioInterface *QAudioInterface::get(QObject *parent)
{
    if (m_onlyInstance != nullptr)
        return m_onlyInstance;
    m_onlyInstance = new QAudioInterface(parent);
    return m_onlyInstance;
}

void QAudioInterface::init()
{
    if (m_audioOutput != nullptr) {
        stop();
        delete m_audioOutput;
        m_audioOutput = nullptr;
    }
    int segCount = (nAudSegCount < 8) ? 8 : nAudSegCount;
    QAudioDeviceInfo info = QAudioDeviceInfo::defaultOutputDevice();
    m_audioOutput = new QAudioOutput(info, m_format, this);
    m_audioOutput->setBufferSize(nAudAllocSegLen * segCount);
}

void QAudioInterface::play()
{
    if (m_audioOutput != nullptr) {
        QIODevice *io = m_audioOutput->start();
        m_buffer = reinterpret_cast<QAudioInterfaceBuffer *>(io);
    }
}

void QAudioInterface::stop()
{
    if (m_audioOutput != nullptr)
        m_audioOutput->stop();
    m_buffer = nullptr;
}

void QAudioInterface::setBuffer(QAudioInterfaceBuffer *buffer)
{
    Q_UNUSED(buffer);
}

void QAudioInterface::setFormat(QAudioFormat &format)
{
    m_format = format;
}

void QAudioInterface::writeMoreData()
{
}

bool QAudioInterface::isReady() const
{
    return (m_audioOutput != nullptr && m_buffer != nullptr);
}

int QAudioInterface::bytesFree() const
{
    return m_audioOutput ? m_audioOutput->bytesFree() : 0;
}

void QAudioInterface::writeAudio(const char *data, int len)
{
    if (m_buffer != nullptr) {
        reinterpret_cast<QIODevice *>(m_buffer)->write(data, len);
    }
}

QAudioInterfaceBuffer::QAudioInterfaceBuffer(int) : QIODevice() {}
QAudioInterfaceBuffer::~QAudioInterfaceBuffer() {}
void QAudioInterfaceBuffer::start() {}
void QAudioInterfaceBuffer::stop() {}
qint64 QAudioInterfaceBuffer::readData(char *, qint64) { return 0; }
qint64 QAudioInterfaceBuffer::writeData(const char *, qint64) { return 0; }
qint64 QAudioInterfaceBuffer::bytesAvailable() { return 0; }
void QAudioInterfaceBuffer::setBufferSize(int) {}
void QAudioInterfaceBuffer::zero() {}
