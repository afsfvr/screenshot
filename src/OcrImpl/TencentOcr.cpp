#include "TencentOcr.h"
#include "../qaesencryption.h"
#include <iostream>
#include <QApplication>
#include <qsslsocket.h>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QEventLoop>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSpacerItem>
#include <QLineEdit>
#include <QMessageBox>
#include <QFormLayout>
#include <QRandomGenerator>
#include <ocr_lib.h>

QVector<Ocr::OcrResult> TencentOcr::ocr(const QImage &img) {
    QVector<Ocr::OcrResult> vector;
    QByteArray bytes;
    QBuffer buffer{&bytes};
    if (! buffer.open(QBuffer::WriteOnly)) {
        return {};
    }
    if (! img.save(&buffer, "png")) {
        buffer.close();
        return {};
    }
    buffer.close();

    const char* res = OcrPredictFromMemory(reinterpret_cast<const unsigned char*>(bytes.constData()), bytes.size());

	//std::cout << std::string(res) << std::endl;
	qDebug() << "ocr result: " << res;

    return vector;
}

bool TencentOcr::init() {

    return true;
}

QWidget *TencentOcr::settingWidget() {
    QWidget *widget = new QWidget;
    widget->setWindowTitle("腾讯云Ocr");

    QLineEdit *id = new QLineEdit{widget};
    QLineEdit *key = new QLineEdit{widget};
    QFormLayout *flayout = new QFormLayout;
    flayout->addRow(new QLabel{"SECRET_ID", widget}, id);
    flayout->addRow(new QLabel{"SECRET_KEY", widget}, key);

    QPushButton *cancel = new QPushButton{"取消", widget};
    QObject::connect(cancel, &QPushButton::clicked, widget, &QWidget::hide);
    QObject::connect(cancel, &QPushButton::clicked, id, &QLineEdit::clear);
    QObject::connect(cancel, &QPushButton::clicked, key, &QLineEdit::clear);
    QPushButton *ok = new QPushButton{"确定", widget};
    QObject::connect(ok, &QPushButton::clicked, widget, [=](){
        QString s1 = id->text().trimmed(), s2 = key->text().trimmed();
        if (s1.isEmpty() || s2.isEmpty()) {
            QMessageBox::warning(widget, "错误", "id或key为空");
        } else {
            m_id = s1;
            m_key = s2;
            id->clear();
            key->clear();
            widget->hide();
        }
    });
    QHBoxLayout *hlayout = new QHBoxLayout;
    hlayout->addItem(new QSpacerItem{40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum});
    hlayout->addWidget(cancel);
    hlayout->addWidget(ok);
    hlayout->addItem(new QSpacerItem{40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum});

    QVBoxLayout *vlayout = new QVBoxLayout(widget);
    vlayout->addLayout(flayout);
    vlayout->addLayout(hlayout);

    return widget;
}

void TencentOcr::restore(QByteArray array) {
    if (array.size() <= 16) return;
    QAESEncryption aes{QAESEncryption::AES_256, QAESEncryption::CBC, QAESEncryption::PKCS7};
    QByteArray key = QCryptographicHash::hash("screenshot", QCryptographicHash::Sha256), iv = array.right(16);
    array.remove(array.size() - 16, 16);
    array = aes.removePadding(aes.decode(array, key, iv));

    if (array.size() < 8) return;
    const char *data = array.constData();
    int len1 = (static_cast<unsigned char>(data[0])) |
              (static_cast<unsigned char>(data[1]) << 8) |
              (static_cast<unsigned char>(data[2]) << 16) |
              (static_cast<unsigned char>(data[3]) << 24);
    if (array.size() < 8 + len1) return;
    QString s1 = QString::fromUtf8(data + 4, len1);

    int len2 = (static_cast<unsigned char>(data[len1 + 4])) |
          (static_cast<unsigned char>(data[len1 + 5]) << 8) |
          (static_cast<unsigned char>(data[len1 + 6]) << 16) |
          (static_cast<unsigned char>(data[len1 + 7]) << 24);
    if (array.size() != 8 + len1 + len2) return;
    QString s2 = QString::fromUtf8(data + len1 + 8, len2);
    m_id = s1;
    m_key = s2;
}

QByteArray TencentOcr::save() {
    QAESEncryption aes{QAESEncryption::AES_256, QAESEncryption::CBC, QAESEncryption::PKCS7};
    QByteArray raw, key = QCryptographicHash::hash("screenshot", QCryptographicHash::Sha256), iv = getRandomByteArray(16);
    QByteArray b1 = m_id.toUtf8(), b2 = m_key.toUtf8();
    raw.append(static_cast<char>(b1.size() & 0xFF))
        .append(static_cast<char>((b1.size() >> 8) & 0xFF))
        .append(static_cast<char>((b1.size() >> 16) & 0xFF))
        .append(static_cast<char>((b1.size() >> 24) & 0xFF))
        .append(b1)
        .append(static_cast<char>(b2.size() & 0xFF))
        .append(static_cast<char>((b2.size() >> 8) & 0xFF))
        .append(static_cast<char>((b2.size() >> 16) & 0xFF))
        .append(static_cast<char>((b2.size() >> 24) & 0xFF))
        .append(b2);

    return aes.encode(raw, key, iv).append(iv);
}

QByteArray TencentOcr::sendRequest(const QString &action, const QByteArray &payload) {
    const QString date = QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd");
    const QString timestamp = QString::number(QDateTime::currentSecsSinceEpoch());

    QByteArray bytes = "POST\n/\n\ncontent-type:application/json; charset=utf-8\nhost:ocr.tencentcloudapi.com\nx-tc-action:";
    bytes.append(action.toLower().toUtf8()).append("\n\ncontent-type;host;x-tc-action\n").append(sha256Hex(payload));

    QByteArray stringToSign = "TC3-HMAC-SHA256\n";
    stringToSign.append(timestamp.toUtf8()).append("\n").append(date.toUtf8()).append("/ocr/tc3_request\n").append(sha256Hex(bytes));

    QByteArray key = "TC3";
    key.append(m_key.toUtf8());
    key = QMessageAuthenticationCode::hash(date.toUtf8(), key, QCryptographicHash::Sha256);
    key = QMessageAuthenticationCode::hash("ocr", key, QCryptographicHash::Sha256);
    key = QMessageAuthenticationCode::hash("tc3_request", key, QCryptographicHash::Sha256);
    key = QMessageAuthenticationCode::hash(stringToSign, key, QCryptographicHash::Sha256).toHex();

    QByteArray authorization = "TC3-HMAC-SHA256 Credential=";
    authorization.append(m_id.toUtf8()).append("/").append(date.toUtf8())
        .append("/ocr/tc3_request, SignedHeaders=content-type;host;x-tc-action, Signature=").append(key);

    QNetworkRequest request;
    request.setUrl(QUrl("http://ocr.tencentcloudapi.com"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json; charset=utf-8");
    request.setRawHeader("Authorization", authorization);
    request.setRawHeader("X-TC-Action", action.toUtf8());
    request.setRawHeader("X-TC-Timestamp", timestamp.toUtf8());
    request.setRawHeader("X-TC-Version", "2018-11-19");
    QNetworkReply *reply = m_manager.post(request, payload);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    return reply->readAll();
}

QByteArray TencentOcr::sha256Hex(const QByteArray &array) {
    QByteArray hash = QCryptographicHash::hash(array, QCryptographicHash::Sha256);
    return hash.toHex();
}

QByteArray TencentOcr::getRandomByteArray(quint32 count) const {
    QByteArray array;
    for (quint32 i = 0; i < count; ++i) {
        array.append(static_cast<char>(QRandomGenerator::global()->generate() % 256));
    }
    return array;
}
