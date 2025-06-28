#include "LongWidget.h"

#include <QPainter>
#include <QApplication>
#include <QScreen>
#include <QHBoxLayout>
#include <QTimerEvent>
#include <QDebug>
#include <opencv2/opencv.hpp>

LongWidget::LongWidget(const QImage &image, const QRect &rect, const QSize &size, QWidget *parent):
    QWidget{parent}, m_widget{nullptr}, m_size{size}, m_id{-1} {
    m_image = image.convertToFormat(QImage::Format_BGR888);

    QRect geometry;
    geometry.setTop(rect.top() - 1);
    geometry.setLeft(rect.left() - 1);
    geometry.setRight(rect.right() + 1);
    geometry.setBottom(rect.bottom() + 1);
    setFixedSize(geometry.size());
    setGeometry(geometry);
    m_screen = rect;

    init();
}

LongWidget::~LongWidget() {
    if (m_id != -1) {
        killTimer(m_id);
        m_id = -1;
    }
    if (m_widget != nullptr) {
        delete m_widget;
        m_widget = nullptr;
    }
}

void LongWidget::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);
    QPainter painter{this};
    QPen pen{Qt::red};
    pen.setWidth(1);
    painter.setPen(pen);
    painter.drawRect(0, 0, width() - 1, height() - 1);
}

void LongWidget::timerEvent(QTimerEvent *event) {
    if (event->timerId() == m_id) {
        join();
    } else {
        QWidget::timerEvent(event);
    }
}

void LongWidget::edit() {
    this->close();
}

void LongWidget::save() {
    m_image.save("/tmp/save.png");
    this->close();
}

void LongWidget::init() {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);

    setVisible(true);
    activateWindow();
    repaint();

    m_widget = new QWidget;
    m_widget->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    m_widget->setAttribute(Qt::WA_DeleteOnClose);

    QHBoxLayout *layout = new QHBoxLayout{m_widget};
    layout->setSpacing(2);
    layout->setContentsMargins(0, 0, 0, 0);

    QPushButton *edit = new QPushButton{m_widget};
    edit->setToolTip("编辑");
    edit->setFixedSize(24, 24);
    edit->setIcon(QIcon(":/images/pencil.png"));
    connect(edit, &QPushButton::clicked, this, &LongWidget::edit);
    layout->addWidget(edit);

    QPushButton *cancel = new QPushButton{m_widget};
    cancel->setToolTip("取消");
    cancel->setFixedSize(24, 24);
    cancel->setIcon(QIcon(":/images/cancel.png"));
    connect(cancel, &QPushButton::clicked, this, &LongWidget::close);
    layout->addWidget(cancel);

    QPushButton *ok = new QPushButton{m_widget};
    ok->setToolTip("保存到剪贴板");
    ok->setFixedSize(24, 24);
    ok->setIcon(QIcon(":/images/ok.png"));
    connect(ok, &QPushButton::clicked, this, &LongWidget::save);
    layout->addWidget(ok);

    m_widget->setFixedSize(76, 24);
    QPoint point{0, 0};
    QRect rect = geometry();
    if (rect.bottom() + 24 <= m_size.height()) {
        point.setY(rect.bottom());
    } else if (rect.top() >= 24) {
        point.setY(rect.top() - 24);
    } else {
        point.setY(rect.top());
    }
    if (rect.left() + 50 <= m_size.width()) {
        point.setX(rect.left());
    } else if (rect.left() >= 50) {
        point.setX(rect.left() - 50);
    }
    m_widget->move(point);
    m_widget->show();
    m_widget->activateWindow();

    m_id = startTimer(1000);
}

void LongWidget::join() {
    QImage image = screenshot();

    cv::Mat matTop(m_image.height(), m_image.width(), CV_8UC3,
                   const_cast<uchar*>(m_image.bits()), m_image.bytesPerLine());
    cv::Mat matBottom(image.height(), image.width(), CV_8UC3,
                      const_cast<uchar*>(image.bits()), image.bytesPerLine());

    int matchHeight = 200;
    if (matBottom.rows < matchHeight) {
        matchHeight = matBottom.rows;
    }
    cv::Mat result;
    cv::matchTemplate(matTop, matBottom(cv::Rect(0, 0, matBottom.cols, matchHeight)), result, cv::TM_CCOEFF_NORMED);

    double minVal, maxVal;
    cv::Point minLoc, maxLoc;
    cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
    int overlapHeight = m_image.height() - maxLoc.y;
    int imageHeight = image.height() - overlapHeight;
    qDebug() << QString("匹配得分：%1，位置：[%2, %3], 重叠高度: %4, 新图使用高度: %5")
                    .arg(maxVal, 0, 'f', 6).arg(maxLoc.x).arg(maxLoc.y).arg(overlapHeight).arg(imageHeight);

    if (imageHeight <= 0) return;
    QImage resultImg(QSize(m_image.width(), m_image.height() + imageHeight), QImage::Format_BGR888);
    QPainter painter(&resultImg);

    painter.drawImage(m_image.rect(), m_image);
    painter.drawImage(QRect(0, m_image.height(), image.width(), imageHeight), image, QRect(0, image.height() - imageHeight, image.width(), imageHeight));

    painter.end();

    m_image = resultImg;
}

QImage LongWidget::screenshot() {
    QList<QScreen*> list = QApplication::screens();
    QSize size;
    for (auto iter = list.cbegin(); iter != list.cend(); ++iter) {
        QRect rect = (*iter)->geometry();
        size.setWidth(std::max(rect.right() + 1, size.width()));
        size.setHeight(std::max(rect.bottom() + 1, size.height()));
        if (rect.contains(m_screen)) {
            return (*iter)->grabWindow(0, m_screen.x(), m_screen.y(), m_screen.width(), m_screen.height()).toImage().convertToFormat(QImage::Format_BGR888);
        }
    }

    QImage image(size, QImage::Format_BGR888);
    QPainter painter(&image);
    for (auto iter = list.cbegin(); iter != list.cend(); ++iter) {
        QScreen *screen = (*iter);
        painter.drawImage(screen->geometry(), screen->grabWindow(0).toImage());
    }
    painter.end();
    return image.copy(m_screen);
}
