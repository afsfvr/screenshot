#ifndef RAPIDOCR_H
#define RAPIDOCR_H

#include "../Ocr.h"

#include <OcrLite.h>
#include <QAtomicInt>
#include <QMutex>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>

class RapidOcrImpl: public OcrBase {
public:
    ~RapidOcrImpl();
    QVector<Ocr::OcrResult> ocr(const QImage &img) override;
    bool init() override;
    bool restore(QByteArray array) override;
    QByteArray save() override;
    void release() override;
    virtual bool hasSettingWidget() override { return true; }
    virtual void showSettingWidget(QWidget *parent) override;

private:
    void _init(const QString &det, const QString &cls, const QString &rec, const QString &key);
    void initWidget(QWidget *parent);

    OcrLite *m_ocr = nullptr;

    QString m_det;
    QString m_cls;
    QString m_rec;
    QString m_key;
    int m_gpuIndexValue = -1;
    int m_numThreadValue = 0;
    int m_paddingValue = 0;
    int m_maxSideLenValue = 0;
    double m_boxScoreThreshValue = 0.0;
    double m_boxThreshValue = 0.0;
    double m_unClipRatioValue = 0.0;
    bool m_doAngleValue = false;
    bool m_mostAngleValue = false;

    bool m_init = false;
    QAtomicInt m_rebuilding = false; // 重建中标志，init() 据此快速失败避免 GUI 阻塞
    QMutex m_mutex;

    QWidget *m_widget = nullptr;
    QLineEdit *m_detEdit = nullptr;
    QLineEdit *m_clsEdit = nullptr;
    QLineEdit *m_recEdit = nullptr;
    QLineEdit *m_keyEdit = nullptr;
    QSpinBox *m_gpuIndex = nullptr;
    QSpinBox *m_numThread = nullptr;
    QSpinBox *m_padding = nullptr;
    QSpinBox *m_maxSideLen = nullptr;
    QDoubleSpinBox *m_boxScoreThresh = nullptr;
    QDoubleSpinBox *m_boxThresh = nullptr;
    QDoubleSpinBox *m_unClipRatio = nullptr;
    QCheckBox *m_doAngle = nullptr;
    QCheckBox *m_mostAngle = nullptr;
};

#endif // RAPIDOCR_H
