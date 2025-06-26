#include "Ocr.h"
#include "TopWidget.h"
#if defined(RAPID_OCR)
#include "OcrImpl/RapidOcr.h"
#elif defined(TENCENT_OCR)
#include "OcrImpl/TencentOcr.h"
#endif

Ocr::Ocr(QObject *parent): QThread{parent}, m_ocr{nullptr} {
    moveToThread(this);
}

Ocr::~Ocr() {
    if (m_ocr) {
        delete m_ocr;
        m_ocr = nullptr;
    }
}

Ocr* Ocr::instance() {
    static Ocr self;
    return &self;
}

void Ocr::ocr(TopWidget *t, const QImage &image) {
    QMetaObject::invokeMethod(this, "_ocr", Qt::QueuedConnection, Q_ARG(TopWidget*, t), Q_ARG(QImage, image));
}

void Ocr::run() {
#if defined(RAPID_OCR)
    m_ocr = new RapidOcr;
#elif defined(TENCENT_OCR)
    m_ocr = new TencentOcr;
#endif
    if (! m_ocr || ! m_ocr->init()) {
        qFatal("ocr初始化失败");
        return;
    }
    exec();
}

void Ocr::_ocr(TopWidget *t, const QImage &img) {
    QVector<Ocr::OcrResult> v;
    if (m_ocr) {
        v = m_ocr->ocr(img);
    }

    QMetaObject::invokeMethod(t, "ocrEnd", Qt::QueuedConnection, Q_ARG(QVector<Ocr::OcrResult>, v));
}
