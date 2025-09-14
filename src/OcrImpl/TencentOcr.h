#ifndef TENCENTOCR_H
#define TENCENTOCR_H

#include "../Ocr.h"

#include <QNetworkAccessManager>

class TencentOcr: public OcrBase {
public:
    QVector<Ocr::OcrResult> ocr(const QImage &img) override;
    bool init() override;
    QWidget *settingWidget() override;
    void restore(QByteArray array) override;
    QByteArray save() override;

private:
    QByteArray sendRequest(const QString &action, const QByteArray &payload);
    QByteArray sha256Hex(const QByteArray &array);
    QByteArray getRandomByteArray(quint32 count) const;

    QNetworkAccessManager m_manager;
    QString m_id;
    QString m_key;
};

#endif // TENCENTOCR_H
