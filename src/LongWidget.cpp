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

// 向下匹配（bigImage底部 和 新图顶部）
static int downMerge(const cv::Mat &grayBig, const cv::Mat &grayNew, int &titleHeight) {
    titleHeight = 0;
    for (int i = 1; i < grayNew.rows; ++i) {
        cv::Mat top1 = grayBig(cv::Rect(0, 0, grayBig.cols, i)), top2 = grayNew(cv::Rect(0, 0, grayNew.cols, i));
        cv::Mat diff, mask;
        cv::absdiff(top1, top2, diff);
        cv::threshold(diff, mask, 1, 255, cv::THRESH_BINARY);
        if (cv::countNonZero(mask) == 0) {
            titleHeight = i;
        } else {
            break;
        }
    }
    const int matchHeight = qMin(grayNew.rows <= 300 ? grayNew.rows / 2 : 200, grayNew.rows - titleHeight);
    cv::Mat bottomResult;
    cv::Mat matchNew = grayNew(cv::Rect(0, titleHeight, grayNew.cols, matchHeight));

    double downMinVal, downMaxVal = 0;
    cv::Point downMinLoc, downMaxLoc;
    if (int difference = (grayBig.rows - grayNew.rows); difference > 0) {
        cv::matchTemplate(grayBig(cv::Rect(0, difference, grayBig.cols, grayNew.rows)), matchNew, bottomResult, cv::TM_CCOEFF_NORMED);
        cv::minMaxLoc(bottomResult, &downMinVal, &downMaxVal, &downMinLoc, &downMaxLoc);
        downMaxLoc.y += difference;
    }
    if (downMaxVal < 0.5) {
        cv::matchTemplate(grayBig, matchNew, bottomResult, cv::TM_CCOEFF_NORMED);
        cv::minMaxLoc(bottomResult, &downMinVal, &downMaxVal, &downMinLoc, &downMaxLoc);
    }

    if (downMaxVal > 0.5) return grayBig.rows - downMaxLoc.y;
    return grayNew.rows;
}

// 向上匹配（bigImage顶部 和 新图底部）
static int upMerge(const cv::Mat &grayBig, const cv::Mat &grayNew) {
    const int matchHeight = grayNew.rows <= 300 ? grayNew.rows / 2 : 200;
    cv::Mat topResult;
    cv::Mat matchNew = grayNew(cv::Rect(0, grayNew.rows - matchHeight, grayNew.cols, matchHeight));

    double upMinVal, upMaxVal = 0;
    cv::Point upMinLoc, upMaxLoc;
    if (grayBig.rows > grayNew.rows) {
        cv::matchTemplate(grayBig(cv::Rect(0, 0, grayBig.cols, grayNew.rows)), matchNew, topResult, cv::TM_CCOEFF_NORMED);
        cv::minMaxLoc(topResult, &upMinVal, &upMaxVal, &upMinLoc, &upMaxLoc);
    }
    if (upMaxVal < 0.5) {
        cv::matchTemplate(grayBig, matchNew, topResult, cv::TM_CCOEFF_NORMED);
        cv::minMaxLoc(topResult, &upMinVal, &upMaxVal, &upMinLoc, &upMaxLoc);
    }

    if (upMaxVal > 0.5) return upMaxLoc.y + matchHeight;
    return grayNew.rows;
}

static void mergePicture(LongWidget *w, QImage *bigImage, QReadWriteLock *lock, BlockQueue<LongWidget::Data> *queue) {
    LongWidget::Data data;
    int titleHeight = 0;
    cv::Mat grayBig;
    lock->lockForRead();
    cv::cvtColor(cv::Mat(bigImage->height(), bigImage->width(), CV_8UC3, bigImage->bits(), bigImage->bytesPerLine()), grayBig, cv::COLOR_BGR2GRAY);
    lock->unlock();
    while (queue->dequeue(&data)) {
        QImage &image = data.image;

        cv::Mat matNew(image.height(), image.width(), CV_8UC3, image.bits(), image.bytesPerLine());
        cv::Mat grayNew;
        cv::cvtColor(matNew, grayNew, cv::COLOR_BGR2GRAY);

        QImage resultImg;
        if (data.down) {
            int downOverlap = downMerge(grayBig, grayNew, titleHeight);
            if (downOverlap < grayNew.rows - titleHeight) {
                resultImg = QImage(QSize(grayBig.cols, grayBig.rows + grayNew.rows - downOverlap - titleHeight), QImage::Format_BGR888);
                QPainter painter(&resultImg);
                lock->lockForRead();
                painter.drawImage(QRect{0, 0, grayBig.cols, grayBig.rows - downOverlap}, *bigImage, QRect{0, 0, grayBig.cols, grayBig.rows - downOverlap});
                lock->unlock();
                painter.drawImage(QRect{0, grayBig.rows - downOverlap, grayNew.cols, grayNew.rows - titleHeight}, image, QRect{0, titleHeight, grayNew.cols, grayNew.rows - titleHeight});
                painter.end();

                cv::Mat tmp;
                cv::vconcat(
                    grayBig(cv::Rect(0, 0, grayBig.cols, grayBig.rows - downOverlap)),
                    grayNew(cv::Rect(0, titleHeight, grayNew.cols, grayNew.rows - titleHeight)),
                    tmp);
                grayBig = tmp;
            }
        } else {
            int upOverlap = upMerge(grayBig, grayNew);
            if (upOverlap < grayNew.rows) {
                resultImg = QImage(QSize(grayBig.cols, grayBig.rows + grayNew.rows - upOverlap), QImage::Format_BGR888);
                QPainter painter(&resultImg);
                painter.drawImage(image.rect(), image);
                lock->lockForRead();
                painter.drawImage(QRect{0, grayNew.rows, grayBig.cols, grayBig.rows - upOverlap},
                                  *bigImage,
                                  QRect{0, upOverlap, grayBig.cols, grayBig.rows - upOverlap});
                lock->unlock();
                painter.end();

                cv::Mat tmp;
                cv::vconcat(grayNew,
                            grayBig(cv::Rect(0, upOverlap, grayBig.cols, grayBig.rows - upOverlap)),
                            tmp);
                grayBig = tmp;
            }
        }

        if (! resultImg.isNull()) {
            lock->lockForWrite();
            bigImage->swap(resultImg);
            lock->unlock();
            QMetaObject::invokeMethod(w, "updateLabel", Qt::QueuedConnection);
        }
    }
}

LongWidget::LongWidget(const QImage &image, const QRect &rect, const QSize &size, QMenu *menu, qreal ratio):
    m_widget{nullptr}, m_size{size}, m_tray_menu{menu}, m_ratio{ratio} {
    m_image = image.convertToFormat(QImage::Format_BGR888);

    m_screen = getScreenRect(rect.adjusted(-1, -1, 1, 1));
    setFixedSize(m_screen.size());
    setGeometry(m_screen);
    m_screen.adjust(1, 1, -1, -1);

    m_action = m_tray_menu->addAction(
        QString("long(%1,%2 %3x%4)")
            .arg(m_screen.x())
            .arg(m_screen.y())
            .arg(m_screen.width() * m_ratio)
            .arg(m_screen.height() * m_ratio),
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
            m_action->setText(QString("long(%1,%2 %3x%4) %5")
                                  .arg(m_screen.x())
                                  .arg(m_screen.y())
                                  .arg(m_screen.width() * m_ratio)
                                  .arg(m_screen.height() * m_ratio)
                                  .arg(m_queue.size()));
        }
        QApplication::processEvents();
        QThread::usleep(20);
    }
    m_lock.lockForWrite();
    if (MainWindow::instance()) {
        MainWindow::instance()->connectTopWidget(new TopWidget(m_image, geometry(), m_tray_menu, m_ratio));
    }

    m_lock.unlock();
    this->close();
}

void LongWidget::save() {
    stop();
    while (! m_queue.isEmpty()) {
        if (m_action) {
            m_action->setText(QString("long(%1,%2 %3x%4) %5")
                                  .arg(m_screen.x())
                                  .arg(m_screen.y())
                                  .arg(m_screen.width() * m_ratio)
                                  .arg(m_screen.height() * m_ratio)
                                  .arg(m_queue.size()));
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
            size.setWidth(qMax(rect.right() + 1, size.width()));
            size.setHeight(qMax(rect.bottom() + 1, size.height()));
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
    update();

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
    QRect rect = m_screen;
    rect.setWidth(rect.width() * m_ratio);
    rect.setHeight(rect.height() * m_ratio);
    for (auto iter = list.cbegin(); iter != list.cend(); ++iter) {
        QRect tmp = (*iter)->geometry();
        qreal ratio = (*iter)->devicePixelRatio();
        tmp.setWidth(tmp.width() * ratio);
        tmp.setHeight(tmp.height() * ratio);
        size.setWidth(std::max<int>(tmp.right() + 1, size.width()));
        size.setHeight(std::max<int>(tmp.bottom() + 1, size.height()));
        if (tmp.contains(rect)) {
            return (*iter)->grabWindow(0)
                .copy((rect.left() - tmp.left()) * m_ratio,
                      (rect.top() - tmp.top()) * m_ratio,
                      rect.width(),
                      rect.height())
                .toImage()
                .convertToFormat(QImage::Format_BGR888);
        }
    }

    QImage image(size, QImage::Format_BGR888);
    QPainter painter(&image);
    for (auto iter = list.cbegin(); iter != list.cend(); ++iter) {
        QScreen *screen = (*iter);
        painter.drawPixmap(screen->geometry(), screen->grabWindow(0));
    }
    painter.end();
    return image.copy(rect);
}

QRect LongWidget::getScreenRect(const QRect &rect) {
    if (m_ratio == 1) return rect;

    QPoint pos = rect.topLeft() * m_ratio;
    QList<QScreen*> list = QApplication::screens();
    QScreen* targetScreen = nullptr;
    for (auto iter = list.cbegin(); iter != list.cend(); ++iter) {
        QRect tmp = (*iter)->geometry();
        qreal ratio = (*iter)->devicePixelRatio();
        tmp.setWidth(tmp.width() * ratio);
        tmp.setHeight(tmp.height() * ratio);
        if (tmp.contains(pos)) {
            targetScreen = *iter;
            break;
        }
    }
    if (! targetScreen) {
        targetScreen = QApplication::primaryScreen();
    }

    QRect targetRect = targetScreen->geometry();
    int x = (pos.x() - targetRect.left()) / m_ratio + targetRect.left();
    int y = (pos.y() - targetRect.top()) / m_ratio + targetRect.top();

    return {x, y, rect.width(), rect.height()};
}
