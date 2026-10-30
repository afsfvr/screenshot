#ifndef OCR_H
#define OCR_H

#include <QImage>
#include <QThread>
#include <QVector>
#include <QPainterPath>
#include <QDebug>
#include <QList>
#include <QMap>
#include <QPair>
#include <QSet>
#include <QMutex>
#include <QWidget>
#include <functional>

class TopWidget;
class OcrBase;

class Ocr: public QThread
{
    Q_OBJECT
    explicit Ocr(QObject *parent = nullptr);
    Q_DISABLE_COPY_MOVE(Ocr)

public:
    struct OcrResult {
        QPainterPath path;
        QString text;
        double score;
    };

    ~Ocr();
    static Ocr* instance();
    bool init();
    void ocr(TopWidget *t, const QImage &image);
    void cancel(const TopWidget *t);

    // 可用的 OCR 后端列表（id, 显示名）
    QVector<QPair<QString, QString>> listBackends() const;
    // 当前选中的后端 id（可能与正在初始化的后端不同，最终以它为准）
    QString backendId() const;
    // 切换后端
    void setBackend(const QString &id);
    // 回填各后端的持久化配置
    void resetConfig(const QMap<QString, QByteArray> &configs);
    // 汇总所有启用后端的配置用于持久化
    QMap<QString, QByteArray> takeConfig();
    // 打开后端自己的设置窗口（id 为空使用当前后端）
    void showSettingWidget(const QString &id, QWidget *parent);
    // 切换离开后是否释放非激活后端的模型内存
    void setAutoRelease(bool enable);

signals:
    void ocrEnd(const QVector<Ocr::OcrResult> &result);

protected:
    void run() override;

private slots:
    void _ocr(TopWidget *t, const QImage &img);

private:
    struct Factory {
        QString id;
        QString name;
        std::function<OcrBase*()> create;
    };

    void _setBackend(const QString &id);
    void _resetConfig(QMap<QString, QByteArray> configs);
    void _ensureImpl(const QString &id);
    // 对已创建的实现应用持久化配置（RapidOCR 可能加载模型，重量级，须在 Ocr 线程、锁外调用）
    void _restoreImpl(const QString &id);

    QVector<Factory> m_factories;
    QMap<QString, OcrBase*> m_impls;
    QMap<QString, QByteArray> m_savedConfigs;
    bool m_configLoaded = false; // 配置是否已加载（由 resetConfig 置位；置位前 takeConfig 不用内存值覆盖）
    QSet<QString> m_ready;       // 已应用配置、可安全序列化的后端 id
    QString m_activeId;
    bool m_autoRelease = true; // 切换离开后释放非激活后端的模型内存
    OcrBase *m_ocr;
    mutable QMutex m_backendMutex;
    QList<TopWidget*> m_list;
    QMutex m_mutex;
};

class OcrBase {
public:
    virtual ~OcrBase() = default;
    virtual QVector<Ocr::OcrResult> ocr(const QImage &img) = 0;
    virtual bool init() { return true; }
    // 返回 false 表示配置解析/应用失败，不计入已就绪
    virtual bool restore(QByteArray array) { Q_UNUSED(array); return true; }
    virtual QByteArray save() { return {}; }
    // 释放该后端的资源
    virtual void release() {}
    virtual bool hasSettingWidget() { return false; }
    virtual void showSettingWidget(QWidget *parent) { Q_UNUSED(parent); }
};

#define OcrInstance (Ocr::instance())
#endif // OCR_H
