#include "GifWidget.h"
#include "Tool.h"

#include <QUuid>
#include <QStandardPaths>
#include <QPainter>
#include <QDebug>
#include <QFileDialog>
#include <QInputDialog>
#include <QApplication>
#include <QScreen>
#include <QDateTime>

GifWidget::GifWidget(const QSize &screenSize, const QRect &rect, QMenu *menu, QWidget *parent): QWidget{parent}, m_timerId{0}, m_size{screenSize} {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    m_tmp = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/" + QUuid::createUuid().toString();
    setGeometry(rect);
    setVisible(true);
    activateWindow();
    repaint();

    initUI();
    m_menu = new QMenu{this};
    m_menu->setTitle(QString("gif(%1,%2 %3x%4)").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height()));
    m_action = m_menu->addAction("开始", this, &GifWidget::buttonClicked);
    m_menu->addAction("关闭", this, &GifWidget::close);
    menu->addMenu(m_menu);

    m_writer = nullptr;
    m_screen.setTop(rect.top() + 1);
    m_screen.setLeft(rect.left() + 1);
    m_screen.setRight(rect.right() - 1);
    m_screen.setBottom(rect.bottom() - 1);
}

GifWidget::~GifWidget() {
    if (m_timerId != 0) {
        killTimer(m_timerId);
        m_timerId = 0;
    }
    if (m_writer != nullptr) {
        GifEnd(m_writer);
        delete m_writer;
        m_writer = nullptr;
    }
    QFile::remove(m_tmp);
    if (m_widget != nullptr) {
        m_widget->disconnect();
        m_widget->close();
        m_widget->deleteLater();
        m_widget = nullptr;
        m_button = nullptr;
        m_spin = nullptr;
        m_label = nullptr;
    }
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
        QImage &&image = screenshot();
        GifWriteFrame(m_writer, image.bits(), m_screen.width(), m_screen.height(), m_delay);
        m_label->setText(QString("%1s").arg((QDateTime::currentMSecsSinceEpoch() - m_startTime) / 1000.0, 0, 'f', 2));
    }
}

void GifWidget::buttonClicked() {
    if (m_writer == nullptr) {
        m_delay = 100 / m_spin->value();
        m_writer = new GifWriter;
        memset(m_writer, 0, sizeof(GifWriter));
        GifBegin(m_writer, m_tmp.toUtf8().data(), m_screen.width(), m_screen.height(), m_delay);
        m_timerId = startTimer(1000 / m_spin->value());
        m_button->setText("结束");
        m_action->setText("结束");
        m_spin->setVisible(false);
        m_label->setText("0");
        m_startTime = QDateTime::currentMSecsSinceEpoch();
    } else {
        if (m_timerId != 0) {
            killTimer(m_timerId);
            m_timerId = 0;
        }
        if (m_writer != nullptr) {
            GifEnd(m_writer);
            delete m_writer;
            m_writer = nullptr;
        }
        QString path = QFileDialog::getSaveFileName(this, "选择路径", Tool::savePath, "*.gif");
        if (! path.isEmpty()) {
            QFileInfo fileinfo{path};
            Tool::savePath = fileinfo.absolutePath();
            if (! fileinfo.fileName().endsWith(".gif", Qt::CaseInsensitive)) {
                path += ".gif";
            }
            QFile::remove(path);
            QFile::rename(m_tmp, path);
        }
        QFile::remove(m_tmp);
        close();
    }
}

void GifWidget::initUI() {
    m_widget = new QWidget;
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
    m_spin->setMaximum(50);
    m_spin->setValue(10);
    m_layout->addWidget(m_spin);

    m_label = new QLabel{m_widget};
    m_label->setText("fps");
    m_layout->addWidget(m_label);

    connect(m_widget, &QWidget::destroyed, this, [=](){
        m_widget = nullptr;
        this->close();
    });
    m_widget->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    m_widget->setWindowOpacity(0.5);
    m_widget->setMinimumSize(105, 25);
    m_widget->setMaximumSize(105, 25);
    QPoint point{0, 0};
    QRect rect = geometry();
    if (rect.bottom() + 25 <= m_size.height()) {
        point.setY(rect.bottom());
    } else if (rect.top() >= 25) {
        point.setY(rect.top() - 25);
    }
    if (rect.left() + 105 <= m_size.width()) {
        point.setX(rect.left());
    } else if (rect.left() >= 105) {
        point.setX(rect.left() - 105);
    }
    m_widget->move(point);
    m_widget->show();
}

QImage GifWidget::screenshot() {
    QList<QScreen*> list = QApplication::screens();
    QSize size;
    for (auto iter = list.cbegin(); iter != list.cend(); ++iter) {
        QRect rect = (*iter)->geometry();
        size.setWidth(std::max(rect.right() + 1, size.width()));
        size.setHeight(std::max(rect.bottom() + 1, size.height()));
        if (rect.contains(m_screen)) {
            return (*iter)->grabWindow(0).copy(m_screen).toImage();
        }
    }

    QImage image(size, QImage::Format_ARGB32);
    QPainter painter(&image);
    for (auto iter = list.cbegin(); iter != list.cend(); ++iter) {
        QScreen *screen = (*iter);
        painter.drawImage(screen->geometry(), screen->grabWindow(0).toImage());
    }
    painter.end();
    return image.copy(m_screen);
}
