#ifndef RAPIDOCR_H
#define RAPIDOCR_H

#include "../Ocr.h"

#include <OcrLite.h>

class RapidOcr: public OcrBase {
public:
    QVector<Ocr::OcrResult> ocr(const QImage &img) override;
    bool init() override;

private:
    OcrLite m_ocr;
};

#endif // RAPIDOCR_H
