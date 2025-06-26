#include "TencentOcr.h"

#include <QApplication>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QEventLoop>

QVector<Ocr::OcrResult> TencentOcr::ocr(const QImage &img) {
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

    QByteArray payload = "{ \"ImageBase64\": \"";
    payload.append(bytes.toBase64()).append("\"} ");

    bytes = sendRequest("GeneralAccurateOCR", payload);
    QJsonObject object = QJsonDocument::fromJson(bytes).object();
    QJsonObject response = object.value("Response").toObject();
    QJsonObject error = response.value("Error").toObject();
    if (! error.isEmpty()) {
        QString code = error.value("Code").toString();
        if (code == "ResourcesSoldOut.ChargeStatusException" ||
            code == "ResourceUnavailable.ResourcePackageRunOut" ||
            code == "ResourceUnavailable.InArrears") {
            object = QJsonDocument::fromJson(sendRequest("GeneralAccurateOCR", payload)).object();
            response = object.value("Response").toObject();
        } else {
            QString message = error.value("Message").toString();
            qWarning() << message;
            return {};
        }
    }

    QVector<Ocr::OcrResult> vector;
    QJsonArray texts = response.value("TextDetections").toArray();
    for (auto iter = texts.cbegin(); iter != texts.cend(); ++iter) {
        QJsonObject obj = (*iter).toObject();
        QString txt = obj.value("DetectedText").toString();
        double score = obj.value("Confidence").toDouble(-1);
        if (txt.isEmpty()) continue;
        QPainterPath path;
        QJsonArray arr = obj.value("Polygon").toArray();
        for (auto iter1 = arr.cbegin(); iter1 != arr.cend(); ++iter1) {
            QJsonObject o = (*iter1).toObject();
            QJsonValue vx = o.value("X");
            QJsonValue vy = o.value("Y");
            if (vx.isDouble() && vy.isDouble()) {
                if (path.elementCount() == 0) {
                    path.moveTo(vx.toDouble(), vy.toDouble());
                } else {
                    path.lineTo(vx.toDouble(), vy.toDouble());
                }
            }
        }
        path.closeSubpath();
        vector.push_back({path, txt, score});
    }
    return vector;
}

bool TencentOcr::init() {
    QStringList list = QApplication::arguments();
    for (auto iter = list.cbegin() + 1; iter != list.cend();) {
        const QString &text = *iter;
        if (text.startsWith("SECRET_ID", Qt::CaseInsensitive)) {
            if (text.length() == 9) {
                ++ iter;
                if (iter != list.cend()) {
                    m_id = *iter;
                    ++ iter;
                }
            } else {
                if (text[9] == '=') {
                    m_id = text.mid(10);
                }
                ++ iter;
            }
        } else if (text.startsWith("SECRET_KEY", Qt::CaseInsensitive)) {
            if (text.length() == 10) {
                ++ iter;
                if (iter != list.cend()) {
                    m_key = *iter;
                    ++ iter;
                }
            } else {
                if (text[10] == '=') {
                    m_key = text.mid(11);
                }
                ++ iter;
            }
        } else {
            ++ iter;
        }
    }
    if (m_id.isEmpty()) {
        m_id = qgetenv("SECRET_ID");
    }
    if (m_key.isEmpty()) {
        m_key = qgetenv("SECRET_KEY");
    }

    if (m_id.isEmpty()) {
        qWarning() << "SECRET_ID is empty";
        return false;
    }
    if (m_key.isEmpty()) {
        qWarning() << "SECRET_KEY is empty";
        return false;
    }
    return true;
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
    request.setUrl(QUrl("https://ocr.tencentcloudapi.com"));
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
    const unsigned char *udata = reinterpret_cast<const unsigned char*>(hash.constData());
    QString ret;
    for (int i = 0; i < hash.size(); ++i) {
        ret.append(QString("%1").arg(static_cast<int>(udata[i]), 2, 16, QLatin1Char('0')));
    }
    return ret.toUtf8();
}
