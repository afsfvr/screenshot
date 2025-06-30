#include <QPainter>
#include <QApplication>
#include <QScreen>
#include <QHBoxLayout>
#include <QTimerEvent>
#include <QThread>
#include <QDebug>
#include <opencv2/opencv.hpp>

#include "LongWidget.h"
#include "TopWidget.h"
#include "mainwindow.h"

static void mergePicture(LongWidget *w, QImage *bigImage, QReadWriteLock *lock, BlockQueue<LongWidget::Data> *queue) {
    LongWidget::Data data;
    while (queue->dequeue(&data)) {
        QImage &image = data.image;
        lock->lockForRead();
        QImage img = bigImage->copy();
        lock->unlock();

        const int matchHeight = std::min(200, image.height());

        cv::Mat matBig(img.height(), img.width(), CV_8UC3, img.bits(), img.bytesPerLine());
        cv::Mat matNew(image.height(), image.width(), CV_8UC3, image.bits(), image.bytesPerLine());

        QImage resultImg;
        if (data.down) {
            // 向下匹配（bigImage底部 和 新图顶部）
            cv::Mat bottomResult;
            cv::matchTemplate(matBig, matNew(cv::Rect(0, 0, matNew.cols, matchHeight)), bottomResult, cv::TM_CCOEFF_NORMED);

            double downMinVal, downMaxVal;
            cv::Point downMinLoc, downMaxLoc;
            cv::minMaxLoc(bottomResult, &downMinVal, &downMaxVal, &downMinLoc, &downMaxLoc);

            int downOverlap = matBig.rows - downMaxLoc.y;
            int downNewHeight = matNew.rows - downOverlap;
            if (downNewHeight > 0 && downMaxVal > 0.5) {
                resultImg = QImage(QSize(matBig.cols, matBig.rows + downNewHeight), QImage::Format_BGR888);
                QPainter painter(&resultImg);
                painter.drawImage(QRect{0, 0, matBig.cols, matBig.rows - downOverlap}, img, QRect{0, 0, matBig.cols, matBig.rows - downOverlap});
                painter.drawImage(QRect{0, matBig.rows - downOverlap, matNew.cols, matNew.rows}, image);
                painter.end();
            }
        } else {
            // 向上匹配（bigImage顶部 和 新图底部）
            cv::Mat topResult;
            cv::matchTemplate(matBig, matNew(cv::Rect(0, matNew.rows - matchHeight, matNew.cols, matchHeight)), topResult, cv::TM_CCOEFF_NORMED);

            double upMinVal, upMaxVal;
            cv::Point upMinLoc, upMaxLoc;
            cv::minMaxLoc(topResult, &upMinVal, &upMaxVal, &upMinLoc, &upMaxLoc);

            int upOverlap = upMaxLoc.y + matchHeight;
            int upNewHeight = matNew.rows - upOverlap;
            if (upNewHeight > 0 && upMaxVal > 0.5) {
                resultImg = QImage(QSize(matBig.cols, matBig.rows + upNewHeight), QImage::Format_BGR888);
                QPainter painter(&resultImg);
                painter.drawImage(image.rect(), image);
                painter.drawImage(QRect{0, matNew.rows, matBig.cols, matBig.rows - upOverlap}, img, QRect{0, upOverlap, matBig.cols, matBig.rows - upOverlap});
                painter.end();
            }
        }

        if (! resultImg.isNull()) {
            lock->lockForWrite();
            *bigImage = resultImg;
            lock->unlock();
            QMetaObject::invokeMethod(w, "updateLabel", Qt::QueuedConnection);
        }
    }
}

LongWidget::LongWidget(const QImage &image, const QRect &rect, const QSize &size, QMenu *menu, MainWindow *main):
    m_widget{nullptr}, m_size{size}, m_tray_menu{menu}, m_main{main} {
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

    m_thread = new std::thread{mergePicture, this, &m_image, &m_lock, &m_queue};

    init();
    show();
}

LongWidget::~LongWidget() {
    stop();
    if (m_thread) {
        while (m_queue.dequeue(nullptr));
        m_thread->join();
        delete m_thread;
        m_thread = nullptr;
    }
    if (m_action) {
        m_action->deleteLater();
        m_action = nullptr;
    }
}

void LongWidget::mouseWheel(bool down) {
    m_queue.enqueue({screenshot(), down});
}

void LongWidget::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);
    QPainter painter{this};
    QPen pen{Qt::red};
    pen.setWidth(1);
    painter.setPen(pen);
    painter.drawRect(0, 0, width() - 1, height() - 1);
}

void LongWidget::edit() {
    stop();
    while (! m_queue.isEmpty()) {
        if (m_action) {
            m_action->setText(QString("long(%1,%2 %3x%4) %5").arg(m_screen.x()).arg(m_screen.y()).arg(m_screen.width()).arg(m_screen.height()).arg(m_queue.size()));
        }
        QApplication::processEvents();
        QThread::usleep(20);
    }
    m_lock.lockForWrite();
    m_main->connectTopWidget(new TopWidget(m_image, geometry(), m_tray_menu));
    m_lock.unlock();
    this->close();
}

void LongWidget::save() {
    stop();
    while (! m_queue.isEmpty()) {
        if (m_action) {
            m_action->setText(QString("long(%1,%2 %3x%4) %5").arg(m_screen.x()).arg(m_screen.y()).arg(m_screen.width()).arg(m_screen.height()).arg(m_queue.size()));
        }
        QApplication::processEvents();
        QThread::usleep(20);
    }
    QClipboard *clipboard = QApplication::clipboard();
    if (clipboard) {
        m_lock.lockForWrite();
        clipboard->setImage(m_image);
        m_lock.unlock();
    }
    this->close();
}

void LongWidget::updateLabel() {
    if (m_label) {
        QList<QScreen*> list = QApplication::screens();
        QSize size;
        for (auto iter = list.cbegin(); iter != list.cend(); ++iter) {
            QRect rect = (*iter)->geometry();
            size.setWidth(std::max(rect.right() + 1, size.width()));
            size.setHeight(std::max(rect.bottom() + 1, size.height()));
        }

        QPoint point;
        QRect geometry = this->geometry();
        int width = geometry.width();
        int right = size.width() - geometry.right();
        size.setWidth(width);
        size.setHeight(geometry.bottom() - 10);

        bool showRight = true;
        if (right > width + 10) {
            showRight = true;
        } else if (geometry.left() > width + 10) {
            showRight = false;
        } else {
            if (geometry.left() > right) {
                showRight = false;
                size.setWidth(geometry.left() - 10);
            } else {
                showRight = true;
                size.setWidth(right - 10);
            }
        }
        m_lock.lockForRead();
        QImage image = m_image.scaled(size, Qt::KeepAspectRatio, Qt::FastTransformation);
        m_lock.unlock();
        point.setX(showRight ? geometry.right() + 10 : geometry.left() - image.width() - 10);
        point.setY(size.height() - image.height() + 11);
        m_label->setFixedSize(image.size());
        m_label->clear();
        m_label->setPixmap(QPixmap::fromImage(image));
        m_label->move(point);
    }
}

void LongWidget::init() {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);

    setVisible(true);
    activateWindow();
    repaint();

    m_widget = new QWidget{this};
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

    m_label = new QLabel{this};
    m_label->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    m_label->setAttribute(Qt::WA_DeleteOnClose);
    m_label->setStyleSheet("border: 1px solid red;");
    connect(m_label, &QWidget::destroyed, this, [this](){
        this->m_label = nullptr;
        this->close();
    });
    m_label->show();
    updateLabel();
}

void LongWidget::stop() {
    hide();
    m_queue.close();
    if (m_widget != nullptr) {
        m_widget->close();
        m_widget->deleteLater();
        m_widget = nullptr;
    }
    if (m_label != nullptr) {
        m_label->close();
        m_label->deleteLater();
        m_label = nullptr;
    }
}

QImage LongWidget::screenshot() {
    QList<QScreen*> list = QApplication::screens();
    QSize size;
    for (auto iter = list.cbegin(); iter != list.cend(); ++iter) {
        QRect rect = (*iter)->geometry();
        size.setWidth(std::max(rect.right() + 1, size.width()));
        size.setHeight(std::max(rect.bottom() + 1, size.height()));
        if (rect.contains(m_screen)) {
            return (*iter)->grabWindow(0, m_screen.x() - rect.left(), m_screen.y() - rect.top(), m_screen.width(), m_screen.height())
                .toImage().convertToFormat(QImage::Format_BGR888);
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
