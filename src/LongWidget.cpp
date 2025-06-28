#include <QPainter>
#include <QApplication>
#include <QScreen>
#include <QHBoxLayout>
#include <QTimerEvent>
#include <QDebug>
#include <opencv2/opencv.hpp>

#include "LongWidget.h"
#include "TopWidget.h"
#include "mainwindow.h"

#ifdef max
#undef max
#endif

static void mergePicture(QImage *bigImage, QReadWriteLock *lock, BlockQueue<QImage> *queue) {
    QImage image;
    while (queue->dequeue(&image)) {
        lock->lockForRead();
        QImage img = *bigImage;
        lock->unlock();
        cv::Mat matTop(img.height(), img.width(), CV_8UC3, img.bits(), img.bytesPerLine());
        cv::Mat matBottom(image.height(), image.width(), CV_8UC3, image.bits(), image.bytesPerLine());

        int matchHeight = 200;
        if (matBottom.rows < matchHeight) {
            matchHeight = matBottom.rows;
        }
        cv::Mat result;
        cv::matchTemplate(matTop, matBottom(cv::Rect(0, 0, matBottom.cols, matchHeight)), result, cv::TM_CCOEFF_NORMED);

        double minVal, maxVal;
        cv::Point minLoc, maxLoc;
        cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
        int overlapHeight = matTop.rows - maxLoc.y;
        int imageHeight = matBottom.rows - overlapHeight;
        // qDebug() << QString("匹配得分：%1，位置：[%2, %3], 重叠高度: %4, 新图使用高度: %5")
        //                 .arg(maxVal, 0, 'f', 6).arg(maxLoc.x).arg(maxLoc.y).arg(overlapHeight).arg(imageHeight);

        if (imageHeight <= 0) continue;
        QImage resultImg(QSize(matTop.cols, matTop.rows + imageHeight), QImage::Format_BGR888);
        QPainter painter(&resultImg);

        painter.drawImage(img.rect(), img);
        painter.drawImage(QRect(0, matTop.rows, matTop.cols, imageHeight), image, QRect(0, matBottom.rows - imageHeight, matTop.cols, imageHeight));

        painter.end();

        lock->lockForWrite();
        *bigImage = resultImg;
        lock->unlock();
    }
}

LongWidget::LongWidget(const QImage &image, const QRect &rect, const QSize &size, QMenu *menu, MainWindow *main):
    m_widget{nullptr}, m_size{size}, m_timer{-1}, m_tray_menu{menu}, m_main{main} {
    m_image = image.convertToFormat(QImage::Format_BGR888);

    QRect geometry;
    geometry.setTop(rect.top() - 1);
    geometry.setLeft(rect.left() - 1);
    geometry.setRight(rect.right() + 1);
    geometry.setBottom(rect.bottom() + 1);
    setFixedSize(geometry.size());
    setGeometry(geometry);
    m_screen = rect;

    m_action = m_tray_menu->addAction(
        QString("long(%1,%2 %3x%4)").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height()),
        this,
        &LongWidget::edit);

    m_thread = new std::thread{mergePicture, &m_image, &m_lock, &m_queue};

    init();
    show();
}

LongWidget::~LongWidget() {
    m_queue.close();
    if (m_thread) {
        m_thread->join();
        delete m_thread;
        m_thread = nullptr;
    }
    if (m_timer != -1) {
        killTimer(m_timer);
        m_timer = -1;
    }
    if (m_widget != nullptr) {
        m_widget->deleteLater();
        m_widget = nullptr;
    }
    if (m_action) {
        m_action->deleteLater();
        m_action = nullptr;
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
    if (event->timerId() == m_timer) {
        m_queue.enqueue(screenshot());
    } else {
        QWidget::timerEvent(event);
    }
}

void LongWidget::edit() {
    m_queue.close();
    m_lock.lockForWrite();
    m_main->connectTopWidget(new TopWidget(m_image, geometry(), m_tray_menu));
    m_lock.unlock();
    this->close();
}

void LongWidget::save() {
    m_queue.close();
    QClipboard *clipboard = QApplication::clipboard();
    if (clipboard) {
        m_lock.lockForWrite();
        clipboard->setImage(m_image);
        m_lock.unlock();
    }
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
    connect(m_widget, &QWidget::destroyed, this, [this](){
        this->m_widget = nullptr;
        this->close();
    });

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

    m_timer = startTimer(500);
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
