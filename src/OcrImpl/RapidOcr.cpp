#include "RapidOcr.h"

#include <QApplication>
#include <QDir>
#include <QtEndian>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSpacerItem>
#include <QMessageBox>
#include <QFileDialog>

RapidOcr::~RapidOcr() {
    m_mutex.lock();
    if (m_ocr) {
        delete m_ocr;
        m_ocr = nullptr;
    }
    m_mutex.unlock();
}

QVector<Ocr::OcrResult> RapidOcr::ocr(const QImage &img) {
    m_mutex.lock();
    if (! m_ocr) {
        QVector<Ocr::OcrResult> v;
        v.push_back({{}, "ocr未初始化", -1});
        m_mutex.unlock();
        return v;
    }
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
    ::OcrResult result = m_ocr->detectBitmap(image.bits(), image.width(), image.height(), 3, padding,
                                             maxSideLen, boxScoreThresh, boxThresh, unClipRatio, doAngle, mostAngle);
    m_mutex.unlock();

    for (auto iter = result.textBlocks.cbegin(); iter != result.textBlocks.cend(); ++iter) {
        QPainterPath path;
        path.moveTo(iter->boxPoint[0].x, iter->boxPoint[0].y);
        path.lineTo(iter->boxPoint[1].x, iter->boxPoint[1].y);
        path.lineTo(iter->boxPoint[2].x, iter->boxPoint[2].y);
        path.lineTo(iter->boxPoint[3].x, iter->boxPoint[3].y);
        path.closeSubpath();
        double score = 0;
        for (auto iter1 = iter->charScores.cbegin(); iter1 != iter->charScores.cend(); ++iter1) {
            score += (*iter1);
        }
        score = iter->charScores.size() == 0 ? -1. : score * 100 / iter->charScores.size();
        v.push_back({path, QString::fromStdString(iter->text), score});
    }

    return v;
}

bool RapidOcr::init() {
    if (m_init) return true;

    if (m_ocr) {
        m_mutex.lock();
        delete m_ocr;
        m_ocr = nullptr;
        m_mutex.unlock();
    }
    m_det = m_cls = m_rec = m_key = "";
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
        qCritical() << "det.onnx不存在";
        return false;
    }
    if (! QFile::exists(cls)) {
        qCritical() << "cls.onnx不存在";
        return false;
    }
    if (! QFile::exists(rec)) {
        qCritical() << "rec.onnx不存在";
        return false;
    }
    if (! QFile::exists(key)) {
        qCritical() << "keys.txt不存在";
        return false;
    }
    _init(det, cls, rec, key);
    return m_init;
}

QWidget *RapidOcr::settingWidget() {
    QWidget *widget = new QWidget;

    QPushButton *det = new QPushButton{"选择路径", widget};
    QPushButton *cls = new QPushButton{"选择路径", widget};
    QPushButton *rec = new QPushButton{"选择路径", widget};
    QPushButton *key = new QPushButton{"选择路径", widget};
    QObject::connect(det, &QPushButton::clicked, widget, [=](){ QString s = QFileDialog::getOpenFileName(widget, "请选择文本检测模型文件（det.onnx）", "", "ONNX模型文件 (*.onnx)"); if (! s.isEmpty()) det->setToolTip(s); });
    QObject::connect(cls, &QPushButton::clicked, widget, [=](){ QString s = QFileDialog::getOpenFileName(widget, "请选择文字方向分类模型文件（cls.onnx）", "", "ONNX模型文件 (*.onnx)"); if (! s.isEmpty()) cls->setToolTip(s); });
    QObject::connect(rec, &QPushButton::clicked, widget, [=](){ QString s = QFileDialog::getOpenFileName(widget, "请选择文本识别模型文件（rec.onnx）", "", "ONNX模型文件 (*.onnx)"); if (! s.isEmpty()) rec->setToolTip(s); });
    QObject::connect(key, &QPushButton::clicked, widget, [=](){ QString s = QFileDialog::getOpenFileName(widget, "请选择字符映射文件（keys.txt）", "", "文本文件 (*.txt)"); if (! s.isEmpty()) key->setToolTip(s); });
    QFormLayout *flayout = new QFormLayout;
    flayout->addRow(new QLabel{"det路径", widget}, det);
    flayout->addRow(new QLabel{"cls路径", widget}, cls);
    flayout->addRow(new QLabel{"rec路径", widget}, rec);
    flayout->addRow(new QLabel{"key路径", widget}, key);

    QPushButton *cancel = new QPushButton{"取消", widget};
    QObject::connect(cancel, &QPushButton::clicked, widget, [=](){ widget->hide(); det->setToolTip(""); cls->setToolTip(""); rec->setToolTip(""); key->setToolTip(""); });
    QPushButton *ok = new QPushButton{"确定", widget};
    QObject::connect(ok, &QPushButton::clicked, widget, [=](){
        QString s1 = det->toolTip(), s2 = cls->toolTip(), s3 = rec->toolTip(), s4 = key->toolTip();

        if (s1.isEmpty() || ! QFile::exists(s1) ||
            s2.isEmpty() || ! QFile::exists(s2) ||
            s3.isEmpty() || ! QFile::exists(s3) ||
            s4.isEmpty() || ! QFile::exists(s4)) {
            QMessageBox::warning(widget, "错误", "部分路径未选择");
        } else {
            widget->hide();
            det->setToolTip("");
            cls->setToolTip("");
            rec->setToolTip("");
            key->setToolTip("");
            _init(s1, s2, s3, s4);
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

void RapidOcr::restore(QByteArray array) {
    const char *data = array.constData();
    int len = array.size();

    QString det = readString(data, len);
    QString cls = readString(data, len);
    QString rec = readString(data, len);
    QString key = readString(data, len);

    _init(det, cls, rec, key);
}

QByteArray RapidOcr::save() {
    QByteArray b1 = m_det.toUtf8(), b2 = m_cls.toUtf8(), b3 = m_rec.toUtf8(), b4 = m_key.toUtf8();
    char data[4];
    QByteArray array;

    qToLittleEndian<int>(b1.size(), data);
    array.append(data, 4).append(b1);

    qToLittleEndian<int>(b2.size(), data);
    array.append(data, 4).append(b2);

    qToLittleEndian<int>(b3.size(), data);
    array.append(data, 4).append(b3);

    qToLittleEndian<int>(b4.size(), data);
    array.append(data, 4).append(b4);

    return array;
}

QString RapidOcr::readString(const char *&data, int &len) {
    if (len < 4) return {};
    int l = qFromLittleEndian<int>(data);
    if (len < 4 + l) return {};
    QString s = QString::fromUtf8(data + 4, l);
    data += 4 + l;
    len -= 4 + l;
    return s.trimmed();
}

void RapidOcr::_init(const QString &det, const QString &cls, const QString &rec, const QString &key) {
    if (det.isEmpty() || ! QFile::exists(det) ||
        cls.isEmpty() || ! QFile::exists(cls) ||
        rec.isEmpty() || ! QFile::exists(rec) ||
        key.isEmpty() || ! QFile::exists(key)) {
        return;
    }

    QMutexLocker locker(&m_mutex);
    if (m_ocr) delete m_ocr;
    m_ocr = new OcrLite;
    m_init = m_ocr->initModels(det.toStdString().c_str(),
                               cls.toStdString().c_str(),
                               rec.toStdString().c_str(),
                               key.toStdString().c_str());
    if (m_init) {
        m_det = det;
        m_cls = cls;
        m_rec = rec;
        m_key = key;
    } else {
        delete m_ocr;
        m_ocr = nullptr;
        m_det = m_cls = m_rec = m_key = "";
    }
}
