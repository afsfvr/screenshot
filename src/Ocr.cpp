#include "Ocr.h"
#include "TopWidget.h"
#if defined(RAPID_OCR)
#include "OcrImpl/RapidOcr.h"
#elif defined(TENCENT_OCR)
#include "OcrImpl/TencentOcr.h"
#endif

Ocr::Ocr(QObject *parent): QThread{parent}, m_ocr{nullptr}, m_setting{nullptr} {
    moveToThread(this);
}

Ocr::~Ocr() {
    m_setting_mutex.lock();
    if (m_setting) {
        m_setting->deleteLater();
        m_setting = nullptr;
    }
    m_setting_mutex.unlock();
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
    if (m_ocr && m_ocr->init()) {
            return true;
    }
    qCritical("ocr初始化失败");
    return false;
}

void Ocr::ocr(TopWidget *t, const QImage &image) {
    if (! init()) {
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

QWidget *Ocr::getSettingWidget() {
    if (! m_ocr) return nullptr;
    if (m_setting) return m_setting;
    m_setting_mutex.lock();
    if (m_setting) return m_setting;
    if (QThread::currentThread() == qApp->thread()) {
        m_setting = m_ocr->settingWidget();
    } else {
        QWidget *widget = nullptr;
        QMetaObject::invokeMethod(qApp, [&](){ widget = m_ocr->settingWidget(); }, Qt::BlockingQueuedConnection);
        m_setting = widget;
    }
    m_setting_mutex.unlock();

    if (m_setting) {
        connect(m_setting, &QWidget::destroyed, this, &Ocr::clearWidget);
    }
    return m_setting;
}

void Ocr::restore(const QByteArray &array) {
    if (m_ocr) m_ocr->restore(array);
}

QByteArray Ocr::save() {
    if (m_ocr) return m_ocr->save();
    return {};
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

void Ocr::clearWidget() {
    QMutexLocker lock{&this->m_mutex};
    this->m_setting = nullptr;
}
