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

    PaddleOCRParams m_params;
    PaddleOCR *m_ocr = nullptr;
};

#endif // PADDLEOCR_H
