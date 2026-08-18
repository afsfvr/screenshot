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
#include <opencv2/opencv.hpp>

PaddleOcrImpl::~PaddleOcrImpl() {
}

QVector<Ocr::OcrResult> PaddleOcrImpl::ocr(const QImage &img) {
    Q_UNUSED(img);
    QVector<Ocr::OcrResult> v;

    return v;
}

bool PaddleOcrImpl::init() {
    return true;
}

void PaddleOcrImpl::restore(QByteArray array) {
    if(array.isEmpty()) {
        qWarning()<<"配置为空";
        return;
    }
}

QByteArray PaddleOcrImpl::save() {
    QByteArray array;

    return array;
}

void PaddleOcrImpl::showSettingWidget(QWidget *parent) {
    Q_UNUSED(parent);
}

void PaddleOcrImpl::initWidget(QWidget *parent) {
    Q_ASSERT(parent != nullptr);
}
