#ifndef RAPIDOCR_H
#define RAPIDOCR_H

#include "../Ocr.h"

#include <OcrLite.h>
#include <QMutex>
#include <QLineEdit>

class RapidOcr: public OcrBase {
public:
    ~RapidOcr();
    QVector<Ocr::OcrResult> ocr(const QImage &img) override;
    bool init() override;
    void restore(QByteArray array) override;
    QByteArray save() override;
    virtual bool hasSettingWidget() override { return true; }
    virtual void showSettingWidget(QWidget *parent) override;

private:
    QString readString(const char *&data, int &len);
    void _init(const QString &det, const QString &cls, const QString &rec, const QString &key);
    void initWidget(QWidget *parent);

    OcrLite *m_ocr = nullptr;

    QString m_det, m_cls, m_rec, m_key;
    bool m_init = false;
    QMutex m_mutex;

    QWidget *m_widget = nullptr;
    QLineEdit *m_detEdit = nullptr;
    QLineEdit *m_clsEdit = nullptr;
    QLineEdit *m_recEdit = nullptr;
    QLineEdit *m_keyEdit = nullptr;
};

#endif // RAPIDOCR_H
