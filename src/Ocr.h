#ifndef OCR_H
#define OCR_H

#include <QImage>
#include <QThread>
#include <QVector>
#include <QPainterPath>
#include <QDebug>
#include <QList>
#include <QMutex>

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
    void ocr(TopWidget *t, const QImage &image);
    void cancel(const TopWidget *t);

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

private:
    OcrBase *m_ocr;
    QList<TopWidget*> m_list;
    QMutex m_mutex;
};

class OcrBase {
public:
    virtual ~OcrBase() = default;
    virtual QVector<Ocr::OcrResult> ocr(const QImage &img) = 0;
    virtual bool init() { return true; }
};

#define OcrInstance (Ocr::instance())
#endif // OCR_H
