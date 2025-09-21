#include "GifWidget.h"
#include "Tool.h"

#include <QUuid>
#include <QStandardPaths>
#include <QPainter>
#include <QDebug>
#include <QFileDialog>
#include <QApplication>
#include <QScreen>
#include <QDateTime>
#include <QTimer>
#include <QComboBox>
#include <QtMath>
#include <malloc.h>

static void writeGIF(BlockQueue<GifFrameData> *queue) {
    GifFrameData data;
    while (queue->dequeue(&data)) {
        if (data.image[0] == 'b') {
            GifWriteFrame(data.writer, data.image + 1, data.width, data.height, data.delay);
        } else if (data.image[0] == 'f') {
            const char *filename = reinterpret_cast<const char*>(data.image + 1);
            QFile file{filename};
            if (file.open(QFile::ReadOnly)) {
                QByteArray array = file.readAll();
                GifWriteFrame(data.writer, reinterpret_cast<const uint8_t*>(array.constData()), data.width, data.height, data.delay);
            }
            QFile::remove(filename);
        }
        delete[] data.image;
    }
}

GifWidget::GifWidget(const QSize &screenSize, const QRect &rect, QMenu *menu, qreal ratio, QWidget *parent):
    QWidget{parent}, m_writer{nullptr}, m_timerId{-1}, m_updateTimerId{-1}, m_size{screenSize}, m_preTime{0}, m_ratio{ratio} {
    m_tmp = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/" + QUuid::createUuid().toString();
    m_screen = getScreenRect(rect.adjusted(-1, -1, 1, 1));
    setFixedSize(m_screen.size());
    setGeometry(m_screen);
    m_screen.adjust(1, 1, -1, -1);

    m_menu = new QMenu{this};
    m_menu->setTitle(QString("gif(%1,%2 %3x%4)")
                         .arg(m_screen.x())
                         .arg(m_screen.y())
                         .arg(m_screen.width() * m_ratio)
                         .arg(m_screen.height() * m_ratio));
    m_action = m_menu->addAction("开始", this, &GifWidget::buttonClicked);
    m_menu->addAction("关闭", this, &GifWidget::close);
    menu->addMenu(m_menu);
    system_menu = menu;

    init();
}

GifWidget::~GifWidget() {
    if (m_timerId != -1) {
        killTimer(m_timerId);
        m_timerId = -1;
    }
    if (m_updateTimerId != -1) {
        killTimer(m_updateTimerId);
        m_updateTimerId = -1;
    }
    if (m_widget != nullptr) {
        delete m_widget;
        m_widget = nullptr;
        m_button = nullptr;
        m_spin = nullptr;
        m_label = nullptr;
        m_box = nullptr;
    }

    m_queue.close();
    QString str = m_menu->title() + " ";
    delete m_menu;
    m_menu = nullptr;
    QAction *action = system_menu->addAction(str + QString::number(m_queue.size()));
    if (m_thread != nullptr) {
        while (! m_queue.isEmpty()) {
            action->setText(str + QString::number(m_queue.size()));
            QApplication::processEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        m_thread->join();
        delete m_thread;
    }
    delete action;

    if (m_writer != nullptr) {
        GifEnd(m_writer);
        delete m_writer;
        m_writer = nullptr;
    }
    if (! m_path.isEmpty()) {
        QFile::remove(m_path);
        QFile::rename(m_tmp, m_path);
    }
    QFile::remove(m_tmp);
#ifdef Q_OS_LINUX
    malloc_trim(0);
#endif
}

void GifWidget::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);
    QPainter painter{this};
    QPen pen{Qt::red};
    pen.setWidth(1);
    painter.setPen(pen);
    painter.drawRect(0, 0, width() - 1, height() - 1);
}

void GifWidget::timerEvent(QTimerEvent *event) {
    if (event->timerId() == m_timerId) {
        updateGIF();
    } else if (event->timerId() == m_updateTimerId) {
        if (m_widget != nullptr) {
            m_label->setText(QString("%1s").arg((QDateTime::currentMSecsSinceEpoch() - m_startTime) / 1000.0, 0, 'f', 2));
        }
    }
}

void GifWidget::buttonClicked() {
    if (m_widget == nullptr) {
        return;
    }
    if (m_writer == nullptr) {
        m_writer = new GifWriter;
        float value = m_spin->value();
        if (m_box->currentIndex() == 0) {
            value = 1 / value;
        }
        m_spin->deleteLater();
        m_spin = nullptr;
        m_box->deleteLater();
        m_box = nullptr;

        m_updateTimerId = startTimer(33);
        m_timerId = startTimer(1000 / value, Qt::PreciseTimer);
        m_button->setText("结束");
        m_action->setText("结束");
        m_label->setText("0s");
        m_label->setVisible(true);

        m_delay = 100 / value;
        memset(m_writer, 0, sizeof(GifWriter));
        m_startTime = QDateTime::currentMSecsSinceEpoch();
        GifBegin(m_writer, m_tmp.toUtf8().data(), qCeil(m_screen.width() * m_ratio), qCeil(m_screen.height() * m_ratio), m_delay);
        updateGIF();
    } else {
        if (m_timerId != -1) {
            killTimer(m_timerId);
            m_timerId = -1;
        }
        if (m_updateTimerId != -1) {
            killTimer(m_updateTimerId);
            m_updateTimerId = -1;
        }
        m_path = QFileDialog::getSaveFileName(this, "选择路径", Tool::savePath, "*.gif");
        if (m_path.isEmpty()) {
            m_queue.close();
            GifFrameData data;
            while (m_queue.dequeue(&data)) {
                if (data.image[0] == 'f') {
                    QFile::remove(reinterpret_cast<const char*>(data.image + 1));
                }
                delete[] data.image;
            }
        } else {
            QFileInfo fileinfo{m_path};
            Tool::savePath = fileinfo.absolutePath();
            if (! fileinfo.fileName().endsWith(".gif", Qt::CaseInsensitive)) {
                m_path += ".gif";
            }
        }
        close();
    }
}

void GifWidget::updateGIF() {
    if (m_writer) {
        QImage &&image = screenshot();
        uint8_t *bits = nullptr;
        if (m_queue.size() > 100) {
            QByteArray array = (QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/" + QUuid::createUuid().toString()).toUtf8();
            QFile file{array};
            if (file.open(QFile::WriteOnly | QFile::Truncate)) {
                file.write(reinterpret_cast<const char*>(image.constBits()), image.sizeInBytes());
                file.close();
                bits = new uint8_t[array.size() + 2];
                memcpy(bits + 1, array.constData(), array.size());
                bits[0] = 'f';
                bits[array.size() + 1] = '\0';
            }
        }
        if (bits == nullptr) {
            bits = new uint8_t[image.sizeInBytes() + 1];
            memcpy(bits + 1, image.constBits(), image.sizeInBytes());
            bits[0] = 'b';
        }

        qint64 time = QDateTime::currentMSecsSinceEpoch();
        int delay = m_delay;
        if (time - m_preTime > delay * 10) {
            delay = qRound((time - m_preTime) / 10.0);
        }
        m_preTime = time;

        m_queue.enqueue({m_writer, bits, image.width(), image.height(), delay});
    }
}

void GifWidget::init() {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);

    setVisible(true);
    activateWindow();
    update();

    m_widget = new QWidget;
    m_widget->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    m_widget->setAttribute(Qt::WA_DeleteOnClose);
    m_layout = new QHBoxLayout{m_widget};
    m_layout->setSpacing(2);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_button = new QPushButton{m_widget};

    m_button->setText("开始");
    m_button->setStyleSheet("background-color: rgba(255, 255, 255, 120); border: none; color: black;");
    connect(m_button, &QPushButton::clicked, this, &GifWidget::buttonClicked);
    m_layout->addWidget(m_button);

    m_spin = new QSpinBox{m_widget};
    m_spin->setMinimum(1);
    m_spin->setMaximum(30);
    m_spin->setValue(2);
    m_layout->addWidget(m_spin);

    m_box = new QComboBox{m_widget};
    m_box->addItem("秒/帧");
    m_box->addItem("帧/秒");
    m_layout->addWidget(m_box);

    m_label = new QLabel{m_widget};
    m_label->setVisible(false);
    m_layout->addWidget(m_label);

    connect(m_widget, &QWidget::destroyed, this, [=](){
        m_widget = nullptr;
        this->close();
    });
    m_widget->setWindowOpacity(0.5);
    m_widget->setFixedSize(140, 25);
    QPoint point{0, 0};
    QRect rect = geometry();
    if (rect.bottom() + 25 <= m_size.height()) {
        point.setY(rect.bottom());
    } else if (rect.top() >= 25) {
        point.setY(rect.top() - 25);
    } else {
        point.setY(rect.top());
    }
    if (rect.left() + 105 <= m_size.width()) {
        point.setX(rect.left());
    } else if (rect.left() >= 105) {
        point.setX(rect.left() - 105);
    }
    m_widget->move(point);
    m_widget->show();

    m_thread = new std::thread{writeGIF, &m_queue};
}

QImage GifWidget::screenshot() {
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
                .convertToFormat(QImage::Format_RGBA8888);
        }
    }

    QImage image(size, QImage::Format_RGBA8888);
    QPainter painter(&image);
    for (auto iter = list.cbegin(); iter != list.cend(); ++iter) {
        QScreen *screen = (*iter);
        painter.drawPixmap(screen->geometry(), screen->grabWindow(0));
    }
    painter.end();
    return image.copy(rect);
}

QRect GifWidget::getScreenRect(const QRect &rect) {
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
