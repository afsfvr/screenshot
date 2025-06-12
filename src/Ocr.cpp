#include "Ocr.h"
#include "TopWidget.h"

#include <QDebug>
#include <QMessageBox>

Ocr::Ocr(QObject *parent): QThread{parent} {
    moveToThread(this);
}

Ocr* Ocr::instance() {
    static Ocr self;
    return &self;
}

void Ocr::ocr(TopWidget *t, const QImage &image) {
    QMetaObject::invokeMethod(this, "rapidOcr", Qt::QueuedConnection, Q_ARG(TopWidget*, t), Q_ARG(QImage, image));
}

void Ocr::run() {
    if (! rapidOcrInit()) {
        qFatal("初始化失败");
        return;
    }
    exec();
}

void Ocr::rapidOcr(TopWidget *t, const QImage &img) {
    QVector<Ocr::OcrResult> v;
    QImage image = img.convertToFormat(QImage::Format_BGR888);

    int shortSide = std::min(image.width(), image.height());
    int longSide = std::max(image.width(), image.height());
    int padding = shortSide < 300 ? 10 : (shortSide < 1000 ? 30 : 50);
    int maxSideLen = longSide > 1500 ? 1280 : 960;
    float boxScoreThresh = (image.width() * image.height() > 1000000) ? 0.5f : 0.6f;
    float boxThresh = 0.3f;
    float unClipRatio = (shortSide < 300) ? 2.5f : 2.0f;
    bool doAngle = false;
    bool mostAngle = doAngle;
    ::OcrResult result = m_ocr.detectBitmap(image.bits(), image.width(), image.height(), 3, padding,
                       maxSideLen, boxScoreThresh, boxThresh, unClipRatio, doAngle, mostAngle);

    for (auto iter = result.textBlocks.cbegin(); iter != result.textBlocks.cend(); ++iter) {
        QPainterPath path;
        path.moveTo(iter->boxPoint[0].x, iter->boxPoint[0].y);
        path.lineTo(iter->boxPoint[1].x, iter->boxPoint[1].y);
        path.lineTo(iter->boxPoint[2].x, iter->boxPoint[2].y);
        path.lineTo(iter->boxPoint[3].x, iter->boxPoint[3].y);
        path.closeSubpath();
        v.push_back({path, QString::fromStdString(iter->text)});
    }

    QMetaObject::invokeMethod(t, "ocrEnd", Qt::QueuedConnection, Q_ARG(QVector<Ocr::OcrResult>, v));
}

bool Ocr::rapidOcrInit() {
    QStringList list = QApplication::arguments();
    QString path = QApplication::applicationDirPath() + "/models";
    if (list.size() > 1) {
        for (auto iter = list.cbegin(); iter != list.cend(); ++iter) {
            QFileInfo info{*iter};
            if (info.exists() && info.isDir()) {
                path = *iter;
                break;
            }
        }
    }
    QString det = QDir::toNativeSeparators(path + "/det.onnx");
    QString cls = QDir::toNativeSeparators(path + "/cls.onnx");
    QString rec = QDir::toNativeSeparators(path + "/rec.onnx");
    QString key = QDir::toNativeSeparators(path + "/keys.txt");
    if (! QFile::exists(det)) {
        QMessageBox::critical(nullptr, "错误", "det.onnx不存在");
        qFatal("det.onnx不存在");
        return false;
    }
    if (! QFile::exists(cls)) {
        QMessageBox::critical(nullptr, "错误", "cls.onnx不存在");
        qFatal("cls.onnx不存在");
        return false;
    }
    if (! QFile::exists(rec)) {
        QMessageBox::critical(nullptr, "错误", "rec.onnx不存在");
        qFatal("rec.onnx不存在");
        return false;
    }
    if (! QFile::exists(key)) {
        QMessageBox::critical(nullptr, "错误", "keys.txt不存在");
        qFatal("keys.txt不存在");
        return false;
    }
    return m_ocr.initModels(det.toStdString().c_str(),
                            cls.toStdString().c_str(),
                            rec.toStdString().c_str(),
                            key.toStdString().c_str());
}
