#include "PaddleOcrImpl.h"

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
#include <QStandardPaths>
#include <QUuid>
#include <QFile>
#include <QMutexLocker>
#include <opencv2/opencv.hpp>
#include <src/pipelines/ocr/result.h>

PaddleOcrImpl::~PaddleOcrImpl() {
    deleteOcr();

    m_widget = nullptr; // m_widget设置了parent，无需delete，而且析构不一定是在ui线程
}

QVector<Ocr::OcrResult> PaddleOcrImpl::ocr(const QImage &img) {
    QMutexLocker locker(&m_mutex);
    QVector<Ocr::OcrResult> v;
    if (! m_ocr) {
        v.push_back({{}, "ocr未初始化", -1});
        return v;
    }

    // QImage -> 临时文件，PaddleOCR 的 Predict 接受文件路径
    QString path = QDir::toNativeSeparators(
        QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + "/" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".png");

    if (img.isNull()) {
        v.push_back({{}, "图像为空", -1});
        return v;
    }

    if (!img.save(path, "PNG")) {
        v.push_back({{}, "保存临时图片失败", -1});
        return v;
    }

    std::vector<std::unique_ptr<BaseCVResult>> results;
    try {
        results = m_ocr->Predict(path.toStdString());
    } catch (const std::string &e) {
        QFile::remove(path);
        qWarning() << "PaddleOCR Predict 异常" << QString::fromStdString(e);
        v.push_back({{}, QString("OCR失败: %1").arg(QString::fromStdString(e)), -1});
        return v;
    } catch (const std::exception &e) {
        QFile::remove(path);
        qWarning() << "PaddleOCR Predict 异常" << e.what();
        v.push_back({{}, QString("OCR失败: %1").arg(QString::fromUtf8(e.what())), -1});
        return v;
    } catch (...) {
        QFile::remove(path);
        qWarning() << "PaddleOCR Predict 未知异常";
        v.push_back({{}, "OCR未知异常", -1});
        return v;
    }

    QFile::remove(path);

    if (results.empty()) {
        return v;
    }

    auto *ocrResult = dynamic_cast<OCRResult*>(results[0].get());
    if (!ocrResult) {
        qWarning() << "PaddleOCR 结果类型转换失败";
        v.push_back({{}, "结果类型错误", -1});
        return v;
    }

    const OCRPipelineResult &pipelineRes = ocrResult->GetPipelineResult();

    size_t count = pipelineRes.rec_texts.size();
    // rec_polys / rec_scores 可能与 rec_texts 不一致时取最小值
    if (pipelineRes.rec_polys.size() < count) count = pipelineRes.rec_polys.size();
    if (pipelineRes.rec_scores.size() < count) count = pipelineRes.rec_scores.size();

    for (size_t i = 0; i < count; ++i) {
        const std::string &txt = pipelineRes.rec_texts[i];
        if (txt.empty()) continue;
        const auto &poly = pipelineRes.rec_polys[i];
        QPainterPath pathPoly;
        if (!poly.empty()) {
            pathPoly.moveTo(poly[0].x, poly[0].y);
            for (size_t j = 1; j < poly.size(); ++j) {
                pathPoly.lineTo(poly[j].x, poly[j].y);
            }
            pathPoly.closeSubpath();
        }
        double score = -1;
        if (i < pipelineRes.rec_scores.size()) {
            score = static_cast<double>(pipelineRes.rec_scores[i]) * 100.0;
        }
        v.push_back({pathPoly, QString::fromStdString(txt), score});
    }

    return v;
}

bool PaddleOcrImpl::init() {
    QMutexLocker locker(&m_mutex);
    if (m_ocr) return true;

    try {
        m_ocr = new PaddleOCR{ m_params };
        return true;
    } catch (const std::string &e) {
        qWarning() << "paddle ocr初始化失败" << QString::fromStdString(e);
    } catch (const std::exception &e) {
        qWarning() << "paddle ocr初始化失败" << e.what();
    } catch (...) {
        qWarning() << "paddle ocr初始化失败 未知异常";
    }
    return false;
}

bool PaddleOcrImpl::_init() {
    QMutexLocker locker(&m_mutex);
    if (m_ocr) {
        delete m_ocr;
        m_ocr = nullptr;
    }
    try {
        qDebug() << "PaddleOcrImpl::init m_params dump:";
        auto dumpOptStr = [](const char* name, const absl::optional<std::string>& opt){
            qDebug() << name << "has" << opt.has_value();
            if (opt.has_value()) {
                const std::string &s = opt.value();
                qDebug() << "  value len" << s.size() << "ptr" << (void*)s.data() << "val:" << QString::fromStdString(s);
            }
        };
        dumpOptStr("doc_ori_name", m_params.doc_orientation_classify_model_name);
        dumpOptStr("doc_ori_dir", m_params.doc_orientation_classify_model_dir);
        dumpOptStr("doc_unwarp_name", m_params.doc_unwarping_model_name);
        dumpOptStr("doc_unwarp_dir", m_params.doc_unwarping_model_dir);
        dumpOptStr("text_det_name", m_params.text_detection_model_name);
        dumpOptStr("text_det_dir", m_params.text_detection_model_dir);
        dumpOptStr("textline_ori_name", m_params.textline_orientation_model_name);
        dumpOptStr("textline_ori_dir", m_params.textline_orientation_model_dir);
        qDebug() << " textline_ori_batch has" << m_params.textline_orientation_batch_size.has_value() << (m_params.textline_orientation_batch_size.has_value()? m_params.textline_orientation_batch_size.value():-1);
        dumpOptStr("text_rec_name", m_params.text_recognition_model_name);
        dumpOptStr("text_rec_dir", m_params.text_recognition_model_dir);
        qDebug() << " text_rec_batch has" << m_params.text_recognition_batch_size.has_value() << (m_params.text_recognition_batch_size.has_value()? m_params.text_recognition_batch_size.value():-1);
        qDebug() << " use_doc_ori has" << m_params.use_doc_orientation_classify.has_value() << (m_params.use_doc_orientation_classify.has_value() ? m_params.use_doc_orientation_classify.value() : -1);
        qDebug() << " use_doc_unwarp has" << m_params.use_doc_unwarping.has_value() << (m_params.use_doc_unwarping.has_value() ? m_params.use_doc_unwarping.value() : -1);
        qDebug() << " use_textline has" << m_params.use_textline_orientation.has_value() << (m_params.use_textline_orientation.has_value() ? m_params.use_textline_orientation.value() : -1);
        qDebug() << " text_det_limit_side has" << m_params.text_det_limit_side_len.has_value() << (m_params.text_det_limit_side_len.has_value()? m_params.text_det_limit_side_len.value():-1);
        dumpOptStr("text_det_limit_type", m_params.text_det_limit_type);
        qDebug() << " text_det_thresh has" << m_params.text_det_thresh.has_value() << (m_params.text_det_thresh.has_value()? m_params.text_det_thresh.value():-1);
        qDebug() << " text_det_box has" << m_params.text_det_box_thresh.has_value() << (m_params.text_det_box_thresh.has_value()? m_params.text_det_box_thresh.value():-1);
        qDebug() << " text_det_unclip has" << m_params.text_det_unclip_ratio.has_value() << (m_params.text_det_unclip_ratio.has_value()? m_params.text_det_unclip_ratio.value():-1);
        qDebug() << " text_rec_score has" << m_params.text_rec_score_thresh.has_value() << (m_params.text_rec_score_thresh.has_value()? m_params.text_rec_score_thresh.value():-1);
        dumpOptStr("lang", m_params.lang);
        dumpOptStr("ocr_version", m_params.ocr_version);
        dumpOptStr("vis_font_dir", m_params.vis_font_dir);
        dumpOptStr("device", m_params.device);
        qDebug() << " enable_mkldnn" << m_params.enable_mkldnn << " mkldnn_cache" << m_params.mkldnn_cache_capacity << " precision" << QString::fromStdString(m_params.precision) << " cpu_threads" << m_params.cpu_threads << " thread_num" << m_params.thread_num;
        qWarning() << " paddlex_config has" << m_params.paddlex_config.has_value();
        m_ocr = new PaddleOCR{ m_params };
        return true;
    } catch (const std::string &e) {
        qWarning() << "paddle ocr初始化失败" << QString::fromStdString(e);
        if (m_ocr) { delete m_ocr; m_ocr = nullptr; }
    } catch (const std::exception &e) {
        qWarning() << "paddle ocr初始化失败" << e.what();
        if (m_ocr) { delete m_ocr; m_ocr = nullptr; }
    }
    return false;
}

void PaddleOcrImpl::restore(QByteArray array) {
    if(array.isEmpty()) {
        qWarning()<<"配置为空";
        return;
    }
    QDataStream stream{&array, QIODevice::ReadOnly};
    stream.setByteOrder(QDataStream::LittleEndian);

    QString rec, recDir, det, detDir, docOri, docOriDir, docUw, docUwDir, lineOri, lineOriDir, precisionValue;
    int lineOriBsValue, recBsValue,  detSideValue, deviceValue, mkldnnCacheValue, threadsValue;
    QString detLimitValue = "max";
    double detThreshValue, detBoxValue, detUnclipValue, recScoreValue;
    bool enableMkldnn;

    stream >> rec >> recDir >> det >> detDir >> docOri >> docOriDir >> docUw
        >> docUwDir >> lineOri >> lineOriDir >> lineOriBsValue >> recBsValue
        >> detSideValue >> detLimitValue >> detThreshValue >> detBoxValue
        >> detUnclipValue >> recScoreValue >> deviceValue >> mkldnnCacheValue
        >> precisionValue >> threadsValue >> enableMkldnn;

    if (stream.status() != QDataStream::Ok) {
        qWarning() << "PaddleOCR配置数据格式错误";
        return;
    }

    QMutexLocker locker(&m_mutex);
    m_params = PaddleOCRParams{};
    if (!docOri.isEmpty()) {
        m_params.doc_orientation_classify_model_name = docOri.toStdString();
    }
    if (!docOriDir.isEmpty()) {
        m_params.doc_orientation_classify_model_dir = docOriDir.toStdString();

        m_params.use_doc_orientation_classify = true;
    } else {
        m_params.use_doc_orientation_classify = false;
    }
    if (!docUwDir.isEmpty()) {
        m_params.doc_unwarping_model_dir = docUwDir.toStdString();

        m_params.use_doc_unwarping = true;
    } else {
        m_params.use_doc_unwarping = false;
    }
    if (!docUw.isEmpty()) {
        m_params.doc_unwarping_model_name = docUw.toStdString();
    }
    if (!detDir.isEmpty()) {
        m_params.text_detection_model_dir = detDir.toStdString();
    }
    if (!det.isEmpty()) {
        m_params.text_detection_model_name = det.toStdString();
    }
    if (!lineOri.isEmpty()) {
        m_params.textline_orientation_model_name = lineOri.toStdString();
    }
    if (!lineOriDir.isEmpty()) {
        m_params.textline_orientation_model_dir = lineOriDir.toStdString();

        m_params.use_textline_orientation = true;
    } else {
        m_params.use_textline_orientation = false;
    }
    m_params.textline_orientation_batch_size = lineOriBsValue;
    if (!rec.isEmpty()) {
        m_params.text_recognition_model_name = rec.toStdString();
    }
    if (!recDir.isEmpty()) {
        m_params.text_recognition_model_dir = recDir.toStdString();
    }

    m_params.text_recognition_batch_size = recBsValue;
    m_params.text_det_limit_side_len = detSideValue;
    m_params.text_det_limit_type = detLimitValue.toStdString();
    m_params.text_det_thresh = static_cast<float>(detThreshValue);
    m_params.text_det_box_thresh = static_cast<float>(detBoxValue);
    m_params.text_det_unclip_ratio = static_cast<float>(detUnclipValue);
    m_params.text_rec_score_thresh = static_cast<float>(recScoreValue);
    if (deviceValue < 0) {
        m_params.device = "cpu";
    } else {
        m_params.device = QString("gpu:%1").arg(deviceValue).toStdString();
    }
    m_params.enable_mkldnn = enableMkldnn;
    m_params.mkldnn_cache_capacity = mkldnnCacheValue;
    m_params.precision = precisionValue.toStdString();
    if (m_params.precision.empty()) m_params.precision = "fp32";
    m_params.cpu_threads = threadsValue;
    m_params.thread_num = 1;
}

QByteArray PaddleOcrImpl::save() {
    QByteArray array;
    QDataStream stream{ &array, QIODevice::WriteOnly };
    stream.setByteOrder(QDataStream::LittleEndian);

    QMutexLocker locker(&m_mutex);
    QString rec = m_params.text_recognition_model_name.has_value() ? QString::fromStdString(m_params.text_recognition_model_name.value()) : "";
    QString recDir = m_params.text_recognition_model_dir.has_value() ? QString::fromStdString(m_params.text_recognition_model_dir.value()) : "";
    QString det = m_params.text_detection_model_name.has_value() ? QString::fromStdString(m_params.text_detection_model_name.value()) : "";
    QString detDir = m_params.text_detection_model_dir.has_value() ? QString::fromStdString(m_params.text_detection_model_dir.value()) : "";
    QString docOri = m_params.doc_orientation_classify_model_name.has_value() ? QString::fromStdString(m_params.doc_orientation_classify_model_name.value()) : "";
    QString docOriDir = m_params.doc_orientation_classify_model_dir.has_value() ? QString::fromStdString(m_params.doc_orientation_classify_model_dir.value()) : "";
    QString docUw = m_params.doc_unwarping_model_name.has_value() ? QString::fromStdString(m_params.doc_unwarping_model_name.value()) : "";
    QString docUwDir = m_params.doc_unwarping_model_dir.has_value() ? QString::fromStdString(m_params.doc_unwarping_model_dir.value()) : "";
    QString lineOri = m_params.textline_orientation_model_name.has_value() ? QString::fromStdString(m_params.textline_orientation_model_name.value()) : "";
    QString lineOriDir = m_params.textline_orientation_model_dir.has_value() ? QString::fromStdString(m_params.textline_orientation_model_dir.value()) : "";
    int lineOriBsValue = m_params.textline_orientation_batch_size.has_value() ? m_params.textline_orientation_batch_size.value() : 6;
    int recBsValue = m_params.text_recognition_batch_size.has_value() ? m_params.text_recognition_batch_size.value() : 6;
    int detSideValue = m_params.text_det_limit_side_len.has_value() ? m_params.text_det_limit_side_len.value() : 960;
    QString detLimitValue = m_params.text_det_limit_type.has_value() ? QString::fromStdString(m_params.text_det_limit_type.value()) : "max";
    double detThreshValue = m_params.text_det_thresh.has_value() ? static_cast<double>(m_params.text_det_thresh.value()) : 0.3;
    double detBoxValue = m_params.text_det_box_thresh.has_value() ? static_cast<double>(m_params.text_det_box_thresh.value()) : 0.6;
    double detUnclipValue = m_params.text_det_unclip_ratio.has_value() ? static_cast<double>(m_params.text_det_unclip_ratio.value()) : 1.5;
    double recScoreValue = m_params.text_rec_score_thresh.has_value() ? static_cast<double>(m_params.text_rec_score_thresh.value()) : 0.0;
    int deviceValue = -1;
    if (m_params.device.has_value()) {
        QString dev = QString::fromStdString(m_params.device.value());
        if (dev.compare("cpu", Qt::CaseInsensitive) == 0) {
            deviceValue = -1;
        } else if (dev.startsWith("gpu:")) {
            bool ok = false;
            int v = dev.mid(4).toInt(&ok);
            deviceValue = ok ? v : -1;
        }
    }
    int mkldnnCacheValue = m_params.mkldnn_cache_capacity;
    QString precisionValue = QString::fromStdString(m_params.precision);
    int threadsValue = m_params.cpu_threads;
    bool enableMkldnn = m_params.enable_mkldnn;

    stream << rec << recDir << det << detDir << docOri << docOriDir << docUw
        << docUwDir << lineOri << lineOriDir << lineOriBsValue << recBsValue
        << detSideValue << detLimitValue << detThreshValue << detBoxValue
        << detUnclipValue << recScoreValue << deviceValue << mkldnnCacheValue
        << precisionValue << threadsValue << enableMkldnn;

    return array;
}

void PaddleOcrImpl::showSettingWidget(QWidget *parent) {
    if (!m_widget) {
        initWidget(parent);
    }

    QMutexLocker locker(&m_mutex);
    if (m_params.text_recognition_model_name.has_value()) {
        m_rec->setText(QString::fromStdString(m_params.text_recognition_model_name.value()));
    } else {
        m_rec->clear();
    }

    if (m_params.text_recognition_model_dir.has_value()) {
        m_recDir->setText(QString::fromStdString(m_params.text_recognition_model_dir.value()));
    } else {
        m_recDir->clear();
    }

    if (m_params.text_detection_model_name.has_value()) {
        m_det->setText(QString::fromStdString(m_params.text_detection_model_name.value()));
    } else {
        m_det->clear();
    }

    if (m_params.text_detection_model_dir.has_value()) {
        m_detDir->setText(QString::fromStdString(m_params.text_detection_model_dir.value()));
    } else {
        m_detDir->clear();
    }

    if (m_params.doc_orientation_classify_model_name.has_value()) {
        m_docOri->setText(QString::fromStdString(m_params.doc_orientation_classify_model_name.value()));
    } else {
        m_docOri->clear();
    }

    if (m_params.doc_orientation_classify_model_dir.has_value()) {
        m_docOriDir->setText(QString::fromStdString(m_params.doc_orientation_classify_model_dir.value()));
    } else {
        m_docOriDir->clear();
    }

    if (m_params.doc_unwarping_model_name.has_value()) {
        m_docUw->setText(QString::fromStdString(m_params.doc_unwarping_model_name.value()));
    } else {
        m_docUw->clear();
    }

    if (m_params.doc_unwarping_model_dir.has_value()) {
        m_docUwDir->setText(QString::fromStdString(m_params.doc_unwarping_model_dir.value()));
    } else {
        m_docUwDir->clear();
    }

    if (m_params.textline_orientation_model_name.has_value()) {
        m_lineOri->setText(QString::fromStdString(m_params.textline_orientation_model_name.value()));
    } else {
        m_lineOri->clear();
    }

    if (m_params.textline_orientation_model_dir.has_value()) {
        m_lineOriDir->setText(QString::fromStdString(m_params.textline_orientation_model_dir.value()));
    } else {
        m_lineOriDir->clear();
    }

    if (m_params.textline_orientation_batch_size.has_value()) {
        m_lineOriBs->setValue(m_params.textline_orientation_batch_size.value());
    }

    if (m_params.text_recognition_batch_size.has_value()) {
        m_recBs->setValue(m_params.text_recognition_batch_size.value());
    }

    if (m_params.text_det_limit_side_len.has_value()) {
        m_detSide->setValue(m_params.text_det_limit_side_len.value());
    }

    if (m_params.text_det_limit_type.has_value()) {
        m_detLimit->setText(QString::fromStdString(m_params.text_det_limit_type.value()));
    }

    if (m_params.text_det_thresh.has_value()) {
        m_detThresh->setValue(m_params.text_det_thresh.value());
    }

    if (m_params.text_det_box_thresh.has_value()) {
        m_detBox->setValue(m_params.text_det_box_thresh.value());
    }

    if (m_params.text_det_unclip_ratio.has_value()) {
        m_detUnclip->setValue(m_params.text_det_unclip_ratio.value());
    }

    if (m_params.text_rec_score_thresh.has_value()) {
        m_recScore->setValue(m_params.text_rec_score_thresh.value());
    }

    if (m_params.device.has_value()) {
        const QString device = QString::fromStdString(m_params.device.value());

        if (device.compare("cpu", Qt::CaseInsensitive) == 0) {
            m_device->setValue(-1);
        } else if (device.startsWith("gpu:")) {
            m_device->setValue(device.mid(4).toInt());
        }
    } else {
        m_device->setValue(-1);
    }

    m_mkldnnCache->setValue(m_params.mkldnn_cache_capacity);

    m_precision->setText(QString::fromStdString(m_params.precision));

    m_threads->setValue(m_params.cpu_threads);

    locker.unlock();
    m_widget->show();
}

void PaddleOcrImpl::initWidget(QWidget *parent) {
    if (m_widget) return;
    Q_ASSERT(parent != nullptr);

    m_widget = new QWidget{parent};
    m_widget->setWindowTitle("PaddleOCR");
    m_widget->setWindowModality(Qt::WindowModal);
    m_widget->setAttribute(Qt::WA_ShowModal, true);
    m_widget->setWindowFlag(Qt::Dialog, true);

    m_rec = new QLineEdit{m_widget};
    m_recDir = new QLineEdit{m_widget};

    m_det = new QLineEdit{m_widget};
    m_detDir = new QLineEdit{m_widget};

    m_docOri = new QLineEdit{m_widget};
    m_docOriDir = new QLineEdit{m_widget};

    m_docUw = new QLineEdit{m_widget};
    m_docUwDir = new QLineEdit{m_widget};

    m_lineOri = new QLineEdit{m_widget};
    m_lineOriDir = new QLineEdit{m_widget};

    m_recDir->setReadOnly(true);
    m_detDir->setReadOnly(true);
    m_docOriDir->setReadOnly(true);
    m_docUwDir->setReadOnly(true);
    m_lineOriDir->setReadOnly(true);

    m_lineOriBs = new QSpinBox{m_widget};
    m_lineOriBs->setRange(1, 128);
    m_lineOriBs->setValue(6);

    m_recBs = new QSpinBox{m_widget};
    m_recBs->setRange(1, 128);
    m_recBs->setValue(6);

    m_detSide = new QSpinBox{m_widget};
    m_detSide->setRange(0, 4096);
    m_detSide->setValue(960);

    m_detLimit = new QLineEdit{"max", m_widget};

    m_detThresh = new QDoubleSpinBox{m_widget};
    m_detThresh->setRange(0.0, 1.0);
    m_detThresh->setSingleStep(0.01);
    m_detThresh->setValue(0.3);

    m_detBox = new QDoubleSpinBox{m_widget};
    m_detBox->setRange(0.0, 1.0);
    m_detBox->setSingleStep(0.01);
    m_detBox->setValue(0.6);

    m_detUnclip = new QDoubleSpinBox{m_widget};
    m_detUnclip->setRange(0.0, 10.0);
    m_detUnclip->setSingleStep(0.01);
    m_detUnclip->setValue(1.5);

    m_recScore = new QDoubleSpinBox{m_widget};
    m_recScore->setRange(0.0, 1.0);
    m_recScore->setSingleStep(0.01);
    m_recScore->setValue(0.0);

    m_device = new QSpinBox{m_widget};
    m_device->setRange(-1, 16);
    m_device->setValue(-1);

    m_mkldnnCache = new QSpinBox{m_widget};
    m_mkldnnCache->setRange(0, 1024);
    m_mkldnnCache->setValue(10);

    m_precision = new QLineEdit{"fp32", m_widget};

    m_threads = new QSpinBox{m_widget};
    m_threads->setRange(1, 128);
    m_threads->setValue(8);

    QGridLayout *layout = new QGridLayout;

    int row = 0;

    QPushButton *detButton = new QPushButton{"选择检测模型目录", m_widget};
    layout->addWidget(new QLabel{"检测模型名称", m_widget}, row, 0);
    layout->addWidget(m_det, row++, 1);
    layout->addWidget(detButton, row, 0);
    layout->addWidget(m_detDir, row++, 1);
    QPushButton *recButton = new QPushButton{"选择识别模型目录", m_widget};
    layout->addWidget(new QLabel{"识别模型名称", m_widget}, row, 0);
    layout->addWidget(m_rec, row++, 1);
    layout->addWidget(recButton, row, 0);
    layout->addWidget(m_recDir, row++, 1);
    QPushButton *docOriButton = new QPushButton{"选择文档方向模型", m_widget};
    layout->addWidget(new QLabel{"文档方向模型名称", m_widget}, row, 0);
    layout->addWidget(m_docOri, row++, 1);
    layout->addWidget(docOriButton, row, 0);
    layout->addWidget(m_docOriDir, row++, 1);
    QPushButton *docUwButton = new QPushButton{"选择去畸变模型", m_widget};
    layout->addWidget(new QLabel{"去畸变模型名称", m_widget}, row, 0);
    layout->addWidget(m_docUw, row++, 1);
    layout->addWidget(docUwButton, row, 0);
    layout->addWidget(m_docUwDir, row++, 1);
    QPushButton *lineOriButton = new QPushButton{"选择文本行方向模型", m_widget};
    layout->addWidget(new QLabel{"文本行方向模型名称", m_widget}, row, 0);
    layout->addWidget(m_lineOri, row++, 1);
    layout->addWidget(lineOriButton, row, 0);
    layout->addWidget(m_lineOriDir, row++, 1);
    layout->addWidget(new QLabel{"文本行方向Batch Size", m_widget}, row, 0);
    layout->addWidget(m_lineOriBs, row++, 1);
    layout->addWidget(new QLabel{"文本识别Batch Size", m_widget}, row, 0);
    layout->addWidget(m_recBs, row++, 1);
    layout->addWidget(new QLabel{"检测最长边", m_widget}, row, 0);
    layout->addWidget(m_detSide, row++, 1);
    layout->addWidget(new QLabel{"检测缩放类型", m_widget}, row, 0);
    layout->addWidget(m_detLimit, row++, 1);
    layout->addWidget(new QLabel{"检测像素阈值", m_widget}, row, 0);
    layout->addWidget(m_detThresh, row++, 1);
    layout->addWidget(new QLabel{"文字框置信度阈值", m_widget}, row, 0);
    layout->addWidget(m_detBox, row++, 1);
    layout->addWidget(new QLabel{"文字框扩张比例", m_widget}, row, 0);
    layout->addWidget(m_detUnclip, row++, 1);
    layout->addWidget(new QLabel{"识别置信度阈值", m_widget}, row, 0);
    layout->addWidget(m_recScore, row++, 1);
    layout->addWidget(new QLabel{"设备（-1 CPU）", m_widget}, row, 0);
    layout->addWidget(m_device, row++, 1);
    layout->addWidget(new QLabel{"MKLDNN Cache（0关闭）", m_widget}, row, 0);
    layout->addWidget(m_mkldnnCache, row++, 1);
    layout->addWidget(new QLabel{"计算精度", m_widget}, row, 0);
    layout->addWidget(m_precision, row++, 1);
    layout->addWidget(new QLabel{"CPU线程数", m_widget}, row, 0);
    layout->addWidget(m_threads, row++, 1);

    auto selectDir = [this](QLineEdit *edit, const QString &title) {
            const QString dir = QFileDialog::getExistingDirectory(m_widget, title, edit->text());

            if (!dir.isEmpty()) {
                edit->setText(dir);
            }
        };

    QObject::connect(detButton, &QPushButton::clicked, m_widget, [=, this]() { selectDir(m_detDir, "选择文本检测模型目录"); });
    QObject::connect(recButton, &QPushButton::clicked, m_widget, [=, this]() { selectDir(m_recDir, "选择文本识别模型目录"); });
    QObject::connect(docOriButton, &QPushButton::clicked, m_widget, [=, this]() { selectDir(m_docOriDir, "选择文档方向分类模型目录"); });
    QObject::connect(docUwButton, &QPushButton::clicked, m_widget, [=, this]() { selectDir(m_docUwDir, "选择文档去畸变模型目录"); });
    QObject::connect(lineOriButton, &QPushButton::clicked, m_widget, [=, this]() { selectDir(m_lineOriDir, "选择文本行方向模型目录"); });

    QPushButton *cancel = new QPushButton{"取消", m_widget};
    QPushButton *ok = new QPushButton{"确定", m_widget};

    QObject::connect(cancel, &QPushButton::clicked, m_widget, &QWidget::hide);

    QObject::connect(ok, &QPushButton::clicked, m_widget, [this]() {
            const QString detDir = m_detDir->text();
            const QString recDir = m_recDir->text();

            if (detDir.isEmpty() || !QDir(detDir).exists()) {
                QMessageBox::warning(m_widget, "错误", "文本检测模型目录无效");
                return;
            }

            if (recDir.isEmpty() || !QDir(recDir).exists()) {
                QMessageBox::warning(m_widget, "错误", "文本识别模型目录无效");
                return;
            }

            PaddleOCRParams params;

            params.text_detection_model_dir = detDir.toStdString();

            params.text_recognition_model_dir = recDir.toStdString();

            if (!m_det->text().isEmpty())
                params.text_detection_model_name = m_det->text().toStdString();

            if (!m_rec->text().isEmpty())
                params.text_recognition_model_name = m_rec->text().toStdString();

            if (!m_docOriDir->text().isEmpty() &&
                QDir(m_docOriDir->text()).exists()) {

                params.doc_orientation_classify_model_dir = m_docOriDir->text().toStdString();

                params.use_doc_orientation_classify = true;

                if (!m_docOri->text().isEmpty()) {
                    params.doc_orientation_classify_model_name = m_docOri->text().toStdString();
                }
            } else {
                params.use_doc_orientation_classify = false;
            }

            if (!m_docUwDir->text().isEmpty() && QDir(m_docUwDir->text()).exists()) {
                params.doc_unwarping_model_dir = m_docUwDir->text().toStdString();
                params.use_doc_unwarping = true;
                if (!m_docUw->text().isEmpty()) {
                    params.doc_unwarping_model_name = m_docUw->text().toStdString();
                }
            } else {
                params.use_doc_unwarping = false;
            }

            if (!m_lineOriDir->text().isEmpty() && QDir(m_lineOriDir->text()).exists()) {
                params.textline_orientation_model_dir = m_lineOriDir->text().toStdString();
                params.use_textline_orientation = true;

                if (!m_lineOri->text().isEmpty()) {
                    params.textline_orientation_model_name = m_lineOri->text().toStdString();
                }
            } else {
                params.use_textline_orientation = false;
            }

            params.textline_orientation_batch_size = m_lineOriBs->value();
            params.text_recognition_batch_size = m_recBs->value();
            params.text_det_limit_side_len = m_detSide->value();
            params.text_det_limit_type = m_detLimit->text().toStdString();
            params.text_det_thresh = static_cast<float>(m_detThresh->value());
            params.text_det_box_thresh = static_cast<float>(m_detBox->value());
            params.text_det_unclip_ratio = static_cast<float>(m_detUnclip->value());

            params.text_rec_score_thresh = static_cast<float>(m_recScore->value());

            if (m_device->value() < 0) {
                params.device = "cpu";
            } else {
                params.device = QString("gpu:%1").arg(m_device->value()).toStdString();
            }

            params.mkldnn_cache_capacity = m_mkldnnCache->value();
            params.enable_mkldnn = m_mkldnnCache->value() > 0;
            params.precision = m_precision->text().trimmed().toStdString();

            if (params.precision.empty()) {
                params.precision = "fp32";
            }

            params.cpu_threads = m_threads->value();

            params.thread_num = 1;

            {
                deleteOcr();
                m_params = params;
            }

            if (!_init()) {
                QMessageBox::warning(m_widget, "错误", "PaddleOCR初始化失败，请检查模型目录、模型版本以及运行环境");
                return;
            }

            m_widget->hide();
        });

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addItem(new QSpacerItem{ 40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum });
    buttonLayout->addWidget(cancel);
    buttonLayout->addWidget(ok);
    buttonLayout->addItem(new QSpacerItem{ 40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum });

    QVBoxLayout *mainLayout = new QVBoxLayout{m_widget};
    mainLayout->addLayout(layout);
    mainLayout->addLayout(buttonLayout);

    m_widget->resize(600, 800);

}

void PaddleOcrImpl::deleteOcr() {
    QMutexLocker locker(&m_mutex);
    if (m_ocr) {
        delete m_ocr;
        m_ocr = nullptr;
    }
}
