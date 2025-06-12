#ifndef OCR_H
#define OCR_H

#include <QThread>
#include <QRect>
#include <QVector>
#include <OcrLite.h>
#include <QPainterPath>

class TopWidget;
class Ocr: public QThread
{
    Q_OBJECT
    explicit Ocr(QObject *parent = nullptr);
    Q_DISABLE_COPY_MOVE(Ocr)

public:
    static Ocr* instance();
    void ocr(TopWidget *t, const QImage &image);

    struct OcrResult {
        QPainterPath path;
        QString text;
    };


signals:
    void ocrEnd(const QVector<Ocr::OcrResult> &result);

protected:
    void run() override;

private slots:
    void rapidOcr(TopWidget *t, const QImage &img);

private:
    bool rapidOcrInit();

    OcrLite m_ocr;
};

#define OcrInstance (Ocr::instance())
#endif // OCR_H
