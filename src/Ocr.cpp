#include "Ocr.h"
#include "TopWidget.h"
#if defined(RAPID_OCR)
#include "OcrImpl/RapidOcr.h"
#elif defined(TENCENT_OCR)
#include "OcrImpl/TencentOcr.h"
#endif

Ocr::Ocr(QObject *parent): QThread{parent}, m_ocr{nullptr}, m_init{false} {
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

bool Ocr::init() {
    if (! m_init && m_ocr) {
        m_init = m_ocr->init();
        if (! m_init) {
            qCritical("ocr初始化失败");
        }
    }
    return m_init;
}

void Ocr::ocr(TopWidget *t, const QImage &image) {
    if (! m_init) {
        QVector<Ocr::OcrResult> v;
        v.push_back({{}, "ocr初始化失败", -1});
        QMetaObject::invokeMethod(t, "ocrEnd", Qt::QueuedConnection, Q_ARG(QVector<Ocr::OcrResult>, v));
        return;
    }
    m_mutex.lock();
    for (auto iter = m_list.cbegin(); iter != m_list.cend(); ++iter) {
        if (*iter == t) {
            m_mutex.unlock();
            return;
        }
    }
    m_list.append(t);
    m_mutex.unlock();
    QMetaObject::invokeMethod(this, "_ocr", Qt::QueuedConnection, Q_ARG(TopWidget*, t), Q_ARG(QImage, image));
}

void Ocr::cancel(const TopWidget *t) {
    m_mutex.lock();
    for (auto iter = m_list.begin(); iter != m_list.end(); ++iter) {
        if (*iter == t) {
            m_list.erase(iter);
            m_mutex.unlock();
            return;
        }
    }
    m_mutex.unlock();
}

void Ocr::run() {
#if defined(RAPID_OCR)
    m_ocr = new RapidOcr;
#elif defined(TENCENT_OCR)
    m_ocr = new TencentOcr;
#endif
    init();
    exec();
}

void Ocr::_ocr(TopWidget *t, const QImage &img) {
    m_mutex.lock();
    int pos = m_list.indexOf(t);
    m_mutex.unlock();
    if (pos == -1) return;
    QVector<Ocr::OcrResult> v;
    if (m_ocr) {
        v = m_ocr->ocr(img);
    }

    m_mutex.lock();
    auto iter = m_list.begin();
    for (; iter != m_list.end(); ++iter) {
        if (*iter == t) {
            m_list.erase(iter);
            QMetaObject::invokeMethod(t, "ocrEnd", Qt::QueuedConnection, Q_ARG(QVector<Ocr::OcrResult>, v));
            break;
        }
    }
    m_mutex.unlock();
}
