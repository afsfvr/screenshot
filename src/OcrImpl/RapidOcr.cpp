#include "RapidOcr.h"

#include <QApplication>
#include <QDir>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSpacerItem>
#include <QMessageBox>
#include <QFileDialog>
#include <QDataStream>
#include <opencv2/opencv.hpp>

RapidOcr::~RapidOcr() {
    QMutexLocker locker{&m_mutex};
    if (m_ocr) {
        delete m_ocr;
        m_ocr = nullptr;
    }
}

QVector<Ocr::OcrResult> RapidOcr::ocr(const QImage &img) {
    QMutexLocker locker{&m_mutex};
    if (! m_ocr) {
        QVector<Ocr::OcrResult> v;
        v.push_back({{}, "ocr未初始化", -1});
        return v;
    }
    QVector<Ocr::OcrResult> v;
    QImage image = img.convertToFormat(QImage::Format_BGR888);
    cv::Mat mat(image.height(), image.width(), CV_8UC3, image.bits(), image.bytesPerLine());

    int shortSide = qMin(image.width(), image.height());
    int longSide = qMax(image.width(), image.height());
    int padding, maxSideLen;
    float boxScoreThresh, boxThresh, unClipRatio;
    if (m_paddingValue > 0) {
        padding = m_paddingValue;
    } else {
        padding = shortSide < 300 ? 10 : (shortSide < 1000 ? 30 : 50);
    }
    if (m_maxSideLenValue > 0) {
        maxSideLen = m_maxSideLenValue;
    } else {
        maxSideLen = longSide > 1500 ? 1280 : 960;
    }
    if (m_boxScoreThreshValue > 0) {
        boxScoreThresh = m_boxScoreThreshValue;
    } else {
        boxScoreThresh = (image.width() * image.height() > 1000000) ? 0.5f : 0.6f;
    }
    if (m_boxThreshValue > 0) {
        boxThresh = m_boxThreshValue;
    } else {
        boxThresh = 0.3f;
    }
    if (m_unClipRatioValue > 0) {
        unClipRatio = m_unClipRatioValue;
    } else {
        unClipRatio = (shortSide < 300) ? 2.5f : 2.0f;
    }
    ::OcrResult result = m_ocr->detect(mat, padding, maxSideLen, boxScoreThresh, 
            boxThresh, unClipRatio, m_doAngleValue, m_mostAngleValue);
    locker.unlock();

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
        score = iter->charScores.size() == 0 ? -1.0 : (score * 100 / iter->charScores.size());
        v.push_back({path, QString::fromStdString(iter->text), score});
    }

    return v;
}

bool RapidOcr::init() {
    if (m_init) return true;

    if (m_ocr) {
        QMutexLocker locker{&m_mutex};
        delete m_ocr;
        m_ocr = nullptr;
    }
    m_det = m_cls = m_rec = m_key = "";
    QString path = QApplication::applicationDirPath() + "/models";
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

void RapidOcr::restore(QByteArray array) {
    if(array.isEmpty()) {
        qWarning()<<"配置为空";
        return;
    }
    QDataStream stream{&array, QIODevice::ReadOnly};
    stream.setByteOrder(QDataStream::LittleEndian);
    QString det, cls, rec, key;
    int gpuIndex, numThread, padding, maxSideLen;
    double boxScoreThresh, boxThresh, unClipRatio;
    bool doAngle ,mostAngle;

    stream >> det >> cls >> rec >> key >> gpuIndex
        >> numThread >> padding >> maxSideLen >> boxScoreThresh
        >> boxThresh >> unClipRatio >> doAngle >> mostAngle;

    if (stream.status() != QDataStream::Ok) {
        qWarning() << "数据格式错误";
        return;
    }

    m_gpuIndexValue = gpuIndex;
    m_numThreadValue = numThread;
    m_paddingValue = padding;
    m_maxSideLenValue = maxSideLen;
    m_boxScoreThreshValue = boxScoreThresh;
    m_boxThreshValue = boxThresh;
    m_unClipRatioValue = unClipRatio;
    m_doAngleValue = doAngle;
    m_mostAngleValue = mostAngle;

    _init(det, cls, rec, key);
}

QByteArray RapidOcr::save() {
    QByteArray array;
    QDataStream stream{ &array, QIODevice::WriteOnly };
    stream.setByteOrder(QDataStream::LittleEndian);

    stream << m_det << m_cls << m_rec << m_key << m_gpuIndexValue
        << m_numThreadValue << m_paddingValue << m_maxSideLenValue
        << m_boxScoreThreshValue << m_boxThreshValue << m_unClipRatioValue
        << m_doAngleValue << m_mostAngleValue;

    return array;
}

void RapidOcr::showSettingWidget(QWidget *parent) {
    if (!m_widget) {
        initWidget(parent);
    }
    m_detEdit->setText(m_det);
    m_clsEdit->setText(m_cls);
    m_recEdit->setText(m_rec);
    m_keyEdit->setText(m_key);
    m_gpuIndex->setValue(m_gpuIndexValue);
    m_numThread->setValue(m_numThreadValue);
    m_padding->setValue(m_paddingValue);
    m_maxSideLen->setValue(m_maxSideLenValue);
    m_boxScoreThresh->setValue(m_boxScoreThreshValue);
    m_boxThresh->setValue(m_boxThreshValue);
    m_unClipRatio->setValue(m_unClipRatioValue);
    m_doAngle->setChecked(m_doAngleValue);
    m_mostAngle->setEnabled(m_doAngleValue);
    m_mostAngle->setChecked(m_mostAngleValue);
    m_widget->show();
}

void RapidOcr::_init(const QString &det, const QString &cls, const QString &rec, const QString &key) {
    m_init = false;
    if (det.isEmpty() || ! QFile::exists(det) ||
        cls.isEmpty() || ! QFile::exists(cls) ||
        rec.isEmpty() || ! QFile::exists(rec) ||
        key.isEmpty() || ! QFile::exists(key)) {
        return;
    }

    QMutexLocker locker{&m_mutex};
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
        m_ocr->setGpuIndex(m_gpuIndexValue);
        if (m_numThreadValue != 0) {
            m_ocr->setNumThread(m_numThreadValue);
        }
    } else {
        delete m_ocr;
        m_ocr = nullptr;
        m_det = m_cls = m_rec = m_key = "";
    }
}

void RapidOcr::initWidget(QWidget *parent) {
    if (m_widget) return;
    Q_ASSERT(parent != nullptr);
    m_widget = new QWidget{parent};
    m_widget->setWindowTitle("RapidOcr");
    m_widget->setWindowModality(Qt::WindowModal);
    m_widget->setAttribute(Qt::WA_ShowModal, true);
    m_widget->setWindowFlag(Qt::Dialog, true);

    QPushButton *det = new QPushButton{"选择det路径", m_widget};
    QPushButton *cls = new QPushButton{"选择cls路径", m_widget};
    QPushButton *rec = new QPushButton{"选择rec路径", m_widget};
    QPushButton *key = new QPushButton{"选择key路径", m_widget};
    m_detEdit = new QLineEdit{m_widget};
    m_detEdit->setReadOnly(true);
    m_clsEdit = new QLineEdit{m_widget};
    m_clsEdit->setReadOnly(true);
    m_recEdit = new QLineEdit{m_widget};
    m_recEdit->setReadOnly(true);
    m_keyEdit = new QLineEdit{m_widget};
    m_keyEdit->setReadOnly(true);
    m_gpuIndex = new QSpinBox{m_widget};
    m_gpuIndex->setRange(-1, 8);
    m_numThread = new QSpinBox{m_widget};
    m_numThread->setRange(0, 128);
    m_padding = new QSpinBox{m_widget};
    m_padding->setRange(0, 128);
    m_maxSideLen = new QSpinBox{m_widget};
    m_maxSideLen->setRange(0, 4096);
    m_boxScoreThresh = new QDoubleSpinBox{m_widget};
    m_boxScoreThresh->setRange(0.0, 1.0);
    m_boxScoreThresh->setSingleStep(0.01);
    m_boxThresh = new QDoubleSpinBox{m_widget};
    m_boxThresh->setRange(0.0, 1.0);
    m_boxThresh->setSingleStep(0.01);
    m_unClipRatio = new QDoubleSpinBox{m_widget};
    m_unClipRatio->setRange(0.0, 5.0);
    m_unClipRatio->setSingleStep(0.01);
    m_doAngle = new QCheckBox{"文字方向检测", m_widget};
    m_mostAngle = new QCheckBox{"整张图片统一方向投票", m_widget};
    QObject::connect(m_doAngle, &QCheckBox::toggled, m_mostAngle, &QCheckBox::setEnabled);
    QObject::connect(det, &QPushButton::clicked, m_widget, [=, this](){ QString s = QFileDialog::getOpenFileName(m_widget, "请选择文本检测模型文件（det.onnx）", "", "ONNX模型文件 (*.onnx)"); if (! s.isEmpty()) m_detEdit->setText(s); });
    QObject::connect(cls, &QPushButton::clicked, m_widget, [=, this](){ QString s = QFileDialog::getOpenFileName(m_widget, "请选择文字方向分类模型文件（cls.onnx）", "", "ONNX模型文件 (*.onnx)"); if (! s.isEmpty()) m_clsEdit->setText(s); });
    QObject::connect(rec, &QPushButton::clicked, m_widget, [=, this](){ QString s = QFileDialog::getOpenFileName(m_widget, "请选择文本识别模型文件（rec.onnx）", "", "ONNX模型文件 (*.onnx)"); if (! s.isEmpty()) m_recEdit->setText(s); });
    QObject::connect(key, &QPushButton::clicked, m_widget, [=, this](){ QString s = QFileDialog::getOpenFileName(m_widget, "请选择字符映射文件（keys.txt）", "", "文本文件 (*.txt)"); if (! s.isEmpty()) m_keyEdit->setText(s); });
    QGridLayout *glayout = new QGridLayout;
    glayout->addWidget(det, 0, 0);
    glayout->addWidget(m_detEdit, 0, 1);
    glayout->addWidget(cls, 1, 0);
    glayout->addWidget(m_clsEdit, 1, 1);
    glayout->addWidget(rec, 2, 0);
    glayout->addWidget(m_recEdit, 2, 1);
    glayout->addWidget(key, 3, 0);
    glayout->addWidget(m_keyEdit, 3, 1);
    glayout->addWidget(new QLabel{"GPU(-1使用CPU)", m_widget}, 4, 0);
    glayout->addWidget(m_gpuIndex, 4, 1);
    glayout->addWidget(new QLabel{"线程数", m_widget}, 5, 0);
    glayout->addWidget(m_numThread, 5, 1);
    glayout->addWidget(new QLabel{"图片边缘扩展", m_widget}, 6, 0);
    glayout->addWidget(m_padding, 6, 1);
    glayout->addWidget(new QLabel{"限制图片最长边尺寸", m_widget}, 7, 0);
    glayout->addWidget(m_maxSideLen, 7, 1);
    glayout->addWidget(new QLabel{"文字框置信度阈值", m_widget}, 8, 0);
    glayout->addWidget(m_boxScoreThresh, 8, 1);
    glayout->addWidget(new QLabel{"文字框内部二值化阈值", m_widget}, 9, 0);
    glayout->addWidget(m_boxThresh, 9, 1);
    glayout->addWidget(new QLabel{"文字框扩张比例", m_widget}, 10, 0);
    glayout->addWidget(m_unClipRatio, 10, 1);
    glayout->addWidget(m_doAngle, 11, 0);
    glayout->addWidget(m_mostAngle, 11, 1);

    QPushButton *cancel = new QPushButton{"取消", m_widget};
    QObject::connect(cancel, &QPushButton::clicked, m_widget, &QWidget::hide);
    QPushButton *ok = new QPushButton{"确定", m_widget};
    QObject::connect(ok, &QPushButton::clicked, m_widget, [=, this]() {
        QString s1 = m_detEdit->text(), s2 = m_clsEdit->text(), s3 = m_recEdit->text(), s4 = m_keyEdit->text();

        if (s1.isEmpty() || ! QFile::exists(s1) ||
            s2.isEmpty() || ! QFile::exists(s2) ||
            s3.isEmpty() || ! QFile::exists(s3) ||
            s4.isEmpty() || ! QFile::exists(s4)) {
            QMessageBox::warning(m_widget, "错误", "部分路径未选择");
        } else {
            m_gpuIndexValue = m_gpuIndex->value();
            m_numThreadValue = m_numThread->value();
            m_paddingValue = m_padding->value();
            m_maxSideLenValue = m_maxSideLen->value();
            m_boxScoreThreshValue = m_boxScoreThresh->value();
            m_boxThreshValue = m_boxThresh->value();
            m_unClipRatioValue = m_unClipRatio->value();
            m_doAngleValue = m_doAngle->isChecked();
            m_mostAngleValue = m_mostAngle->isChecked();
            m_widget->hide();
            _init(s1, s2, s3, s4);
            if (! m_init) {
                QMessageBox::warning(m_widget, "错误", "初始化失败，请检查模型文件是否正确");
            }
        }
    });
    QHBoxLayout *hlayout = new QHBoxLayout;
    hlayout->addItem(new QSpacerItem{40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum});
    hlayout->addWidget(cancel);
    hlayout->addWidget(ok);
    hlayout->addItem(new QSpacerItem{40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum});

    QVBoxLayout *vlayout = new QVBoxLayout(m_widget);
    vlayout->addLayout(glayout);
    vlayout->addLayout(hlayout);

    m_widget->resize(360, 480);
}
