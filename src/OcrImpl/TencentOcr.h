#ifndef TENCENTOCR_H
#define TENCENTOCR_H

#include "../Ocr.h"

#include <QNetworkAccessManager>
#include <QLineEdit>

class TencentOcr: public OcrBase {
public:
    QVector<Ocr::OcrResult> ocr(const QImage &img) override;
    bool init() override;
    void restore(QByteArray array) override;
    QByteArray save() override;
    virtual bool hasSettingWidget() override { return true; }
    virtual void showSettingWidget(QWidget *parent) override;

private:
    QByteArray sendRequest(const QString &action, const QByteArray &payload);
    QByteArray sha256Hex(const QByteArray &array);
    QByteArray getRandomByteArray(quint32 count) const;
    void initWidget(QWidget *parent);

    QNetworkAccessManager m_manager;
    QString m_id;
    QString m_key;

    QWidget *m_widget = nullptr;
    QLineEdit *m_idEdit = nullptr;
    QLineEdit *m_keyEdit = nullptr;
};

#endif // TENCENTOCR_H
