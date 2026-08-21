#ifndef PADDLEOCR_H
#define PADDLEOCR_H

#include "../Ocr.h"

#include <QMutex>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <src/api/pipelines/base.h>
#include <src/api/pipelines/ocr.h>

class PaddleOcrImpl: public OcrBase {
public:
    ~PaddleOcrImpl();
    QVector<Ocr::OcrResult> ocr(const QImage &img) override;
    bool init() override;
    void restore(QByteArray array) override;
    QByteArray save() override;
    virtual bool hasSettingWidget() override { return true; }
    virtual void showSettingWidget(QWidget *parent) override;

private:
    void initWidget(QWidget *parent);
    void deleteOcr();
    bool _init();

    PaddleOCRParams m_params;
    PaddleOCR *m_ocr = nullptr;
    QMutex m_mutex;

    QWidget *m_widget = nullptr;
    QLineEdit *m_rec = nullptr; // 文本识别模型名称
    QLineEdit *m_recDir = nullptr; // 文本识别模型路径
    QLineEdit *m_det = nullptr; // 文本检测模型名称
    QLineEdit *m_detDir = nullptr; // 文本检测模型路径
    QLineEdit *m_docOri = nullptr; // 文档方向分类模型名称
    QLineEdit *m_docOriDir = nullptr; // 文档方向分类模型路径
    QLineEdit *m_docUw = nullptr; // 文档去畸变模型名称
    QLineEdit *m_docUwDir = nullptr; // 文档去畸变模型路径
    QLineEdit *m_lineOri = nullptr; // 文本行方向分类模型名称
    QLineEdit *m_lineOriDir = nullptr; // 文本行方向分类模型路径
    QSpinBox *m_lineOriBs = nullptr; // 文本行方向分类模型的批处理大小
    QSpinBox *m_recBs = nullptr; // 文本识别模型一次批量处理多少个文字行
    QSpinBox *m_detSide = nullptr; // 文本检测阶段对输入图片尺寸进行缩放时的边长限制值
    QLineEdit *m_detLimit = nullptr;
    QDoubleSpinBox *m_detThresh = nullptr; // 文本检测模型的像素级阈值
    QDoubleSpinBox *m_detBox = nullptr; // 文字框置信度阈值
    QDoubleSpinBox *m_detUnclip = nullptr; // 文本检测框扩张比例
    QDoubleSpinBox *m_recScore = nullptr; // 文本识别结果的置信度阈值
    QSpinBox *m_device = nullptr;
    QSpinBox *m_mkldnnCache = nullptr; // 0不使用mkldnn
    QLineEdit *m_precision = nullptr;
    QSpinBox *m_threads = nullptr;
};

#endif // PADDLEOCR_H
