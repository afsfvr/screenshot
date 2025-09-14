#ifndef OCR_H
#define OCR_H

#include <QImage>
#include <QThread>
#include <QVector>
#include <QPainterPath>
#include <QDebug>
#include <QList>
#include <QMutex>
#include <QWidget>

class TopWidget;
class OcrBase;
class Ocr: public QThread
{
    Q_OBJECT
    explicit Ocr(QObject *parent = nullptr);
    Q_DISABLE_COPY_MOVE(Ocr)

public:
    ~Ocr();
    static Ocr* instance();
    bool init();
    void ocr(TopWidget *t, const QImage &image);
    void cancel(const TopWidget *t);
    QWidget *getSettingWidget();
    void restore(const QByteArray &array);
    QByteArray save();

    struct OcrResult {
        QPainterPath path;
        QString text;
        double score;
    };


signals:
    void ocrEnd(const QVector<Ocr::OcrResult> &result);

protected:
    void run() override;

private slots:
    void _ocr(TopWidget *t, const QImage &img);
    void clearWidget();

private:
    OcrBase *m_ocr;
    QList<TopWidget*> m_list;
    QMutex m_mutex;
    bool m_init;
    QWidget *m_setting;
    QMutex m_setting_mutex;
};

class OcrBase {
public:
    virtual ~OcrBase() = default;
    virtual QVector<Ocr::OcrResult> ocr(const QImage &img) = 0;
    virtual bool init() { return true; }
    virtual void restore(QByteArray array) { Q_UNUSED(array); }
    virtual QByteArray save() { return {}; }
    virtual QWidget *settingWidget() { return nullptr; }
};

#define OcrInstance (Ocr::instance())
#endif // OCR_H
