#include "Ocr.h"
#include "TopWidget.h"
#include <QApplication>
#include <QPointer>
#if defined(RAPID_OCR)
#include "OcrImpl/RapidOcrImpl.h"
#endif
#if defined(TENCENT_OCR)
#include "OcrImpl/TencentOcrImpl.h"
#endif
#if defined(PADDLE_OCR)
#include "OcrImpl/PaddleOcrImpl.h"
#endif

Ocr::Ocr(QObject *parent): QThread{parent}, m_ocr{nullptr} {
    moveToThread(this);
#if defined(TENCENT_OCR)
    m_factories.push_back({ QStringLiteral("tencent"), QStringLiteral("腾讯云OCR"), []() { return static_cast<OcrBase*>(new TencentOcrImpl); } });
#endif
#if defined(RAPID_OCR)
    m_factories.push_back({ QStringLiteral("rapid"), QStringLiteral("RapidOCR"), []() { return static_cast<OcrBase*>(new RapidOcrImpl); } });
#endif
#if defined(PADDLE_OCR)
    m_factories.push_back({ QStringLiteral("paddle"), QStringLiteral("PaddleOCR"), []() { return static_cast<OcrBase*>(new PaddleOcrImpl); } });
#endif
    if (! m_factories.isEmpty()) {
        m_activeId = m_factories.first().id;
    }
}

Ocr::~Ocr() {
    QMutexLocker locker(&m_backendMutex);
    qDeleteAll(m_impls);
}

Ocr* Ocr::instance() {
    static Ocr self;
    return &self;
}

QVector<QPair<QString, QString>> Ocr::listBackends() const {
    QVector<QPair<QString, QString>> list;
    for (const Factory &f : m_factories) {
        list.push_back({ f.id, f.name });
    }
    return list;
}

QString Ocr::backendId() const {
    QMutexLocker locker(&m_backendMutex);
    return m_activeId;
}

void Ocr::setBackend(const QString &id) {
    {
        QMutexLocker locker(&m_backendMutex);
        m_activeId = id;
    }
    QMetaObject::invokeMethod(this, [this, id]() { _setBackend(id); }, Qt::QueuedConnection);
}

void Ocr::showSettingWidget(const QString &id, QWidget *parent) {
    QString backend = id;
    if (backend.isEmpty()) {
        backend = backendId();
    }
    if (backend.isEmpty()) {
        return;
    }
    QMetaObject::invokeMethod(this, [this, backend, parent]() {
        _ensureImpl(backend);
        OcrBase *o = nullptr;
        {
            QMutexLocker locker(&m_backendMutex);
            o = m_impls.value(backend);
        }
        if (o && o->hasSettingWidget()) {
            // 窗口须在 GUI 线程创建显示；QPointer 防 parent 已销毁
            QPointer<QWidget> widget(parent);
            QMetaObject::invokeMethod(qApp, [o, widget]() {
                if (widget) {
                    o->showSettingWidget(widget);
                }
            }, Qt::QueuedConnection);
        }
    }, Qt::QueuedConnection);
}

QMap<QString, QByteArray> Ocr::takeConfig() {
    QMutexLocker locker(&m_backendMutex);
    QMap<QString, QByteArray> map = m_savedConfigs;
    // 配置尚未加载（坏文件/版本不兼容）：禁止用内存实例覆盖，原样返回已保存的配置
    if (! m_configLoaded) {
        return map;
    }
    for (auto it = m_impls.cbegin(); it != m_impls.cend(); ++it) {
        // 未就绪的后端（尚未应用过持久化配置）不得用内存值覆盖磁盘配置
        if (! m_ready.contains(it.key())) continue;
        map[it.key()] = it.value()->save();
    }
    return map;
}

bool Ocr::init() {
    // 快照后锁外 init：模型加载重，不阻塞其他线程的 backendId()/takeConfig()
    OcrBase *ocr = nullptr;
    {
        QMutexLocker locker(&m_backendMutex);
        ocr = m_ocr;
    }
    if (ocr && ocr->init()) {
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

void Ocr::run() {
    QThread::sleep(1);
    QString id;
    {
        QMutexLocker locker(&m_backendMutex);
        id = m_activeId;
    }
    if (! id.isEmpty()) {
        _setBackend(id);
    }
    exec();
}

void Ocr::_ensureImpl(const QString &id) {
    // 仅在 Ocr 线程调用；restore 可能加载模型（数秒），放锁外
    if (! m_impls.contains(id)) {
        const Factory *factory = nullptr;
        for (const Factory &f : m_factories) {
            if (f.id == id) {
                factory = &f;
                break;
            }
        }
        if (! factory) {
            qWarning() << "未启用的 OCR 后端：" << id;
            return;
        }
        OcrBase *impl = factory->create();
        if (! impl) {
            qCritical() << "创建 OCR 后端失败：" << id;
            return;
        }
        QMutexLocker locker(&m_backendMutex);
        m_impls.insert(id, impl);
        if (m_activeId == id) {
            m_ocr = impl;
        }
    }
    // 已就绪时内部直接跳过，不会重复 restore
    _restoreImpl(id);
}

void Ocr::_restoreImpl(const QString &id) {
    // 仅在 Ocr 线程、锁外调用（可能触发模型加载）
    QByteArray blob;
    OcrBase *impl = nullptr;
    {
        QMutexLocker locker(&m_backendMutex);
        impl = m_impls.value(id);
        if (! impl || m_ready.contains(id)) return;
        if (m_configLoaded) {
            blob = m_savedConfigs.value(id);
        }
    }
    bool ok = true;
    if (! blob.isEmpty()) {
        ok = impl->restore(blob);
        qDebug() << "恢复 OCR 配置：" << id << "bytes:" << blob.size() << "ok:" << ok;
    }
    // restore 失败不入 ready，takeConfig 继续沿用磁盘配置
    {
        QMutexLocker locker(&m_backendMutex);
        if (m_configLoaded && ok) {
            m_ready.insert(id);
        }
    }
}

void Ocr::_resetConfig(QMap<QString, QByteArray> configs) {
    QList<QString> ids;
    {
        QMutexLocker locker(&m_backendMutex);
        m_savedConfigs = configs;
        m_configLoaded = true;
        ids = m_impls.keys();
    }
    for (const QString &id : ids) {
        _restoreImpl(id);
    }
    // Paddle restore 会删掉提前构建的引擎，补一次 init 在 Ocr 线程重建，避免首次截图阻塞 GUI
    init();
}

void Ocr::resetConfig(const QMap<QString, QByteArray> &configs) {
    QMetaObject::invokeMethod(this, [this, configs]() { _resetConfig(configs); }, Qt::QueuedConnection);
}

void Ocr::setAutoRelease(bool enable) {
    QMutexLocker locker(&m_backendMutex);
    m_autoRelease = enable;
}

void Ocr::_setBackend(const QString &id) {
    _ensureImpl(id);
    QList<OcrBase*> oldImpls;
    {
        QMutexLocker locker(&m_backendMutex);
        auto it = m_impls.constFind(id);
        if (it == m_impls.constEnd()) {
            qCritical("切换OCR后端失败：%s", id.toUtf8().constData());
            return;
        }
        m_activeId = id;
        m_ocr = *it;
        // 新后端挂好后再释放旧的
        if (m_autoRelease) {
            for (auto jt = m_impls.constBegin(); jt != m_impls.constEnd(); ++jt) {
                if (jt.key() != id) {
                    oldImpls.append(*jt);
                }
            }
        }
    }
    // release 可能销毁模型会话（数秒），锁外执行；就绪状态保留，save() 仍可序列化最新配置
    for (OcrBase *old : oldImpls) {
        old->release();
    }
    // 兜底：确保该实例应用过持久化配置（已就绪时内部直接跳过）
    _restoreImpl(id);
    qDebug() << "切换 OCR 后端：" << id;
    init();
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
