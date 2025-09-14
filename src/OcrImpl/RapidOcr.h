#ifndef RAPIDOCR_H
#define RAPIDOCR_H

#include "../Ocr.h"

#include <OcrLite.h>
#include <QMutex>

class RapidOcr: public OcrBase {
public:
    ~RapidOcr();
    QVector<Ocr::OcrResult> ocr(const QImage &img) override;
    bool init() override;
    QWidget *settingWidget() override;
    void restore(QByteArray array) override;
    QByteArray save() override;

private:
    QString readString(const char *&data, int &len);
    void _init(const QString &det, const QString &cls, const QString &rec, const QString &key);

    OcrLite *m_ocr = nullptr;

    QString m_det, m_cls, m_rec, m_key;
    bool m_init = false;
    QMutex m_mutex;
};

#endif // RAPIDOCR_H
