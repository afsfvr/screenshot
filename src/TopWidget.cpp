﻿#include "TopWidget.h"

#include <QTimer>
#include <QtMath>
#include <QHBoxLayout>
#include <QPushButton>

#ifdef Q_OS_LINUX
#include <X11/Xlib.h>
#include <QX11Info>
#endif
#ifdef KeyPress
#undef KeyPress
#endif
TopWidget::TopWidget(QImage &&image, QVector<Shape *> &vector, const QRect &rect, QMenu *menu) {
    m_image = std::move(image);
    m_vector = std::move(vector);
    tray_menu = menu;
    QSize size = rect.size();
    setFixedSize(size);
    setGeometry(rect);
#ifdef OCR
    m_center = QPoint(size.width() / 2, size.height() / 2);
    m_radius = qMin(size.width(), size.height()) / 4;
    m_radius1 = qRound(m_radius / 8.0);
#endif
    init();
}

TopWidget::TopWidget(QImage &image, QPainterPath &&path, QVector<Shape *> &vector, const QRect &rect, QMenu *menu) {
    m_image = std::move(image);
    m_path = std::move(path);
    m_vector = std::move(vector);
    tray_menu = menu;
    QSize size = rect.size();
    setFixedSize(size);
    setGeometry(rect);
#ifdef OCR
    m_center = QPoint(size.width() / 2, size.height() / 2);
    m_radius = qMin(size.width(), size.height()) / 4;
    m_radius1 = qRound(m_radius / 8.0);
#endif
    init();
}

TopWidget::~TopWidget() {
    delete m_menu;
#ifdef OCR
    if (m_widget != nullptr) {
        delete m_widget;
        m_widget = nullptr;
        m_text = nullptr;
        m_button = nullptr;
    }
#endif
}

void TopWidget::showTool() {
    QPoint point;

    QRect rect = geometry();
    QList<QScreen*> list = QApplication::screens();
    QSize size;
    for (auto iter = list.cbegin(); iter != list.cend(); ++iter) {
        QRect rect = (*iter)->geometry();
        size.setWidth(std::max(rect.right(), size.width()));
        size.setHeight(std::max(rect.bottom(), size.height()));
    }

    if (rect.bottom() + m_tool->height() < size.height()) {
        if (rect.right() > size.width()) { // 左下
            if (rect.left() + m_tool->width() > size.width()) {
                point.setX(rect.left() - m_tool->width());
            } else {
                point.setX(rect.left());
            }
        } else { // 右下
            if (rect.right() >= m_tool->width()) {
                point.setX(rect.right() - m_tool->width());
            } else {
                point.setX(rect.right());
            }
        }
        point.setY(rect.bottom());
    } else if (rect.top() >= m_tool->height()) {
        if (rect.right() > size.width()) { // 左上
            if (rect.left() + m_tool->width() > size.width()) {
                point.setX(rect.left() - m_tool->width());
            } else {
                point.setX(rect.left());
            }
        } else { // 右上
            if (rect.right() >= m_tool->width()) {
                point.setX(rect.right() - m_tool->width());
            } else {
                point.setX(rect.right());
            }
        }
        point.setY(rect.top() - m_tool->height());
    }
    m_tool->show();
    m_tool->move(point);
}

#ifdef OCR
void TopWidget::ocrStart() {
    m_ocr.clear();
    if (m_ocr_timer != -1) {
        killTimer(m_ocr_timer);
        m_ocr_timer = -1;
        repaint();
        return;
    }
    m_angle = 0;
    m_ocr_timer = startTimer(50);
    OcrInstance->ocr(this, grab().toImage());
}

void TopWidget::ocrEnd(const QVector<Ocr::OcrResult> &result) {
    m_ocr = result;

    if (m_ocr_timer != -1) {
        killTimer(m_ocr_timer);
        m_ocr_timer = -1;
        repaint();
    }
}
#endif

#ifdef Q_OS_LINUX
void TopWidget::mouseRelease(QSharedPointer<QMouseEvent> event) {
    if (m_move && event->button() == Qt::LeftButton) {
        QPoint screenPos = event->globalPos();
        QList<QScreen*> list = QApplication::screens();
        if (list.size() > 1) {
            for (auto iter = list.cbegin(); iter != list.cend(); ++iter) {
                QRect rect = (*iter)->geometry();
                if (rect.contains(screenPos)) {
                    screenPos -= rect.topLeft();
                    break;
                }
            }
        }
        QMouseEvent *e = new QMouseEvent(QEvent::MouseButtonRelease, event->globalPos() - geometry().topLeft(), event->globalPos(), screenPos, Qt::LeftButton, event->buttons(), event->modifiers(), event->source());
        QApplication::postEvent(this, e);
    }
}
#endif

#ifdef OCR
bool TopWidget::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_widget) {
        if (event->type() == QEvent::Leave) {
            hideWidget();
        } else if (event->type() == QEvent::KeyPress){
            QKeyEvent *e = dynamic_cast<QKeyEvent*>(event);
            if (e && e->key() == Qt::Key_Escape) {
                hideWidget();
            }
        }
    }
    return BaseWindow::eventFilter(watched, event);
}

void TopWidget::timerEvent(QTimerEvent *event) {
    if (event->timerId() == m_ocr_timer) {
        m_angle = (m_angle + 30) % 360;
        repaint();
    } else {
        BaseWindow::timerEvent(event);
    }
}
#endif

void TopWidget::keyPressEvent(QKeyEvent *event) {
    BaseWindow::keyPressEvent(event);
    if (event->modifiers() == Qt::NoModifier) {
        switch (event->key()) {
        case Qt::Key_Left:
            move(this->x() - 1, this->y());
            showTool();
            break;
        case Qt::Key_Right:
            move(this->x() + 1, this->y());
            showTool();
            break;
        case Qt::Key_Up:
            move(this->x(), this->y() - 1);
            showTool();
            break;
        case Qt::Key_Down:
            move(this->x(), this->y() + 1);
            showTool();
            break;
        }
    }
}

void TopWidget::closeEvent(QCloseEvent *event) {
    qInfo() << QString("关闭置顶窗口:(%1,%2 %3x%4)").arg(x()).arg(y()).arg(m_image.width()).arg(m_image.height());
    m_tool->close();
    QWidget::closeEvent(event);
    this->deleteLater();
}

void TopWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    if (m_path.elementCount() < 2) {
        painter.drawImage(rect(), m_image);
    } else {
        painter.fillPath(m_path, m_image);
        painter.setClipPath(m_path);
    }

    for (auto iter = m_vector.cbegin(); iter != m_vector.cend(); ++iter) {
        (*iter)->draw(painter);
    }
    if (m_shape != nullptr) {
        m_shape->draw(painter);
    }

#ifdef OCR
    if (m_ocr_timer != -1) {
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(this->rect(), QColor(255, 255, 255, 150));
        for (int i = 0; i < 12; ++i) {
            int alpha = 255 - (i * 20);
            QColor color(100, 100, 255, qMax(0, alpha));
            painter.setBrush(color);
            painter.setPen(Qt::NoPen);

            float theta = (m_angle + i * 30) * M_PI / 180.0;
            int x = m_center.x() + qCos(theta) * m_radius;
            int y = m_center.y() + qSin(theta) * m_radius;

            painter.drawEllipse(QPoint(x, y), m_radius1, m_radius1);
        }
    }
    painter.setPen(Qt::red);
    for (auto iter = m_ocr.cbegin(); iter != m_ocr.cend(); ++iter) {
        painter.drawPath(iter->path);
    }
#endif

    drawTips(painter);
}

void TopWidget::hideEvent(QHideEvent *event) {
    m_tool->hide();
    BaseWindow::hideEvent(event);
}

void TopWidget::mousePressEvent(QMouseEvent *event) {
    if (event->buttons() == Qt::LeftButton) {
        m_press = true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        m_point = event->globalPosition().toPoint() - geometry().topLeft();
#else
        m_point = event->globalPos() - geometry().topLeft();
#endif
        if (m_tool->isDraw()) {
            setCursorShape(Qt::BitmapCursor);
#ifdef OCR
            if (m_ocr_timer == -1) {
                setShape(event->pos());
            }
#else
            setShape(event->pos());
#endif
        } else {
            setCursorShape(Qt::SizeAllCursor);
            m_tool->hide();
        }
    }
}

void TopWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        if (m_cursor == Qt::BitmapCursor) {
            if (m_shape == nullptr) {
            } else if (! m_shape->isNull()){
                m_vector.push_back(m_shape);
                m_shape = nullptr;
                clearStack();
            } else {
                safeDelete(m_shape);
            }
            repaint();
        }
#ifdef Q_OS_LINUX
        m_move = false;
#endif
        m_press = false;
        showTool();
    }
}

void TopWidget::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() == Qt::NoButton) {
        if (m_tool->isDraw()) {
            setCursorShape(Qt::BitmapCursor);
        } else {
            setCursorShape(Qt::SizeAllCursor);
        }
#ifdef OCR
        bool hide = true;
        for (auto iter = m_ocr.cbegin(); iter != m_ocr.cend(); ++iter) {
            if (iter->path.contains(event->pos())) {
                QString ptr = QString::number(reinterpret_cast<quintptr>(&(iter->text)));
                if (ptr != m_text->statusTip()) {
                    QRect rect = iter->path.boundingRect().toRect();
                    m_text->setReadOnly(true);
                    m_text->setText(iter->text);
                    m_text->setStatusTip(ptr);
                    m_widget->setFixedWidth(std::max(55, rect.width()));
                    m_widget->show();
                    m_widget->move(mapToGlobal(rect.bottomLeft()));
                }
                hide = false;
                break;
            }
        }
        if (hide && ! m_widget->geometry().contains(event->globalPos())) {
            hideWidget();
        }
#endif
    } else if (m_cursor == Qt::BitmapCursor) {
        const QRect &rect = this->rect();
        if (m_shape != nullptr && rect.isValid()) {
            QPoint point = event->pos();
            if (rect.contains(point)) {
                m_shape->addPoint(point);
            } else {
                if (point.x() < rect.left()) {
                    point.setX(rect.left());
                } else if (point.x() > rect.right()) {
                    point.setX(rect.right());
                }
                if (point.y() < rect.top()) {
                    point.setY(rect.top());
                } else if (point.y() > rect.bottom()) {
                    point.setY(rect.bottom());
                }
                m_shape->addPoint(point);
            }
        }
        repaint();
    } else if (m_cursor == Qt::SizeAllCursor && m_press) {
#if defined (Q_OS_LINUX)
        m_move = true;

        XEvent xevent;
        memset(&xevent, 0, sizeof(XEvent));
        Display *display = QX11Info::display();
        xevent.xclient.type = ClientMessage;
        xevent.xclient.message_type = XInternAtom(display, "_NET_WM_MOVERESIZE", False);
        xevent.xclient.display = display;
        xevent.xclient.window = this->winId();
        xevent.xclient.format = 32;
        xevent.xclient.data.l[0] = event->globalX();
        xevent.xclient.data.l[1] = event->globalY();
        xevent.xclient.data.l[2] = 8;
        xevent.xclient.data.l[3] = Button1;
        xevent.xclient.data.l[4] = 1;
        XUngrabPointer(display, CurrentTime);
        XSendEvent(display, QX11Info::appRootWindow(QX11Info::appScreen()), False, SubstructureNotifyMask | SubstructureRedirectMask, &xevent);
        XFlush(display);
#elif defined (Q_OS_WINDOWS)
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        move(event->globalPosition().toPoint() - m_point);
#else
        move(event->globalPos() - m_point);
#endif
#endif
    }
}

void TopWidget::moveEvent(QMoveEvent *event) {
    Q_UNUSED(event);
    QString text = QString("(%1,%2 %3x%4)").arg(x()).arg(y()).arg(m_image.width()).arg(m_image.height());
    m_menu->setTitle(text);
}

void TopWidget::focusOutEvent(QFocusEvent *event) {
    Q_UNUSED(event);
    QObject *o = QApplication::focusObject();
    if (o == this) {
        return;
    }

    while (o != nullptr && (o = o->parent()) != nullptr) {
        if (o == this) {
            return;
        }
    }
    m_tool->hide();
}

QRect TopWidget::getGeometry() const {
    return this->geometry();
}

void TopWidget::save(const QString &path) {
    if (m_path.isEmpty()) {
        m_image = this->grab().toImage();
    } else {
        QPainter painter(&m_image);
        painter.setClipPath(m_path);
        for (auto iter = m_vector.cbegin(); iter != m_vector.cend(); ++iter) {
            (*iter)->draw(painter);
        }
        if (m_shape != nullptr) {
            m_shape->draw(painter);
        }
        painter.end();
    }
    if (path.length() == 0) {
        QClipboard *clipboard = QApplication::clipboard();
        if (clipboard) {
            clipboard->setImage(m_image);
        }
    } else {
        m_image.save(path);
    }
    this->close();
}

void TopWidget::end() {
    m_tool->close();
    this->close();
}

void TopWidget::topChange(bool top) {
    m_tool->hide();
    this->hide();
    setWindowFlag(Qt::WindowStaysOnTopHint, top);
    m_tool->setWindowFlag(Qt::WindowStaysOnTopHint, top);

    QTimer::singleShot(100, this, &TopWidget::moveTop);
}

void TopWidget::moveTop() {
    this->show();
    showTool();
    this->activateWindow();
    this->raise();
}

#ifdef OCR
void TopWidget::copyText() {
    if (m_text) {
        QString text = m_text->toPlainText();
        if (! text.isEmpty()) {
            QClipboard *clipboard = QApplication::clipboard();
            if (clipboard) {
                clipboard->setText(text);
                addTip("复制成功");
            } else {
                addTip("复制失败");
            }
        }
    }
}

void TopWidget::editText() {
    if (m_text) {
        if (m_text->isReadOnly()) {
            m_text->setReadOnly(false);
            m_text->setFocus();
            m_text->moveCursor(QTextCursor::End);
            if (m_button) {
                m_button->setIcon(QIcon(":/images/ok.png"));
            }
        } else {
            m_text->setReadOnly(true);
            if (m_button) {
                m_button->setIcon(QIcon(":/images/pencil.png"));
            }
            for (auto iter = m_ocr.begin(); iter != m_ocr.end(); ++iter) {
                QString ptr = QString::number(reinterpret_cast<quintptr>(&(iter->text)));
                if (ptr == m_text->statusTip()) {
                    iter->text = m_text->toPlainText();
                    break;
                }
            }
        }
    }
}
#endif

void TopWidget::init() {
    setWindowFlag(Qt::Tool, false);
    setFocusPolicy(Qt::ClickFocus);
    setMouseTracking(true);

    m_tool->topChange();
    connect(m_tool, &Tool::clickTop, m_tool, &Tool::topChange);
    connect(m_tool, &Tool::topChanged, this, &TopWidget::topChange);

    QString text = QString("(%1,%2 %3x%4)").arg(x()).arg(y()).arg(m_image.width()).arg(m_image.height());
    // QString text = QString("0x%1(%2,%3 %4x%5)").arg(reinterpret_cast<quintptr>(this), QT_POINTER_SIZE, 16).arg(x()).arg(y()).arg(m_image.width()).arg(m_image.height());

    m_menu = new QMenu(this);
    m_menu->setIcon(QIcon(QPixmap::fromImage(m_image)));
    m_menu->setTitle(text);
    m_menu->addAction("显示", this, &TopWidget::moveTop);
    m_menu->addAction("关闭", this, &TopWidget::close);
    tray_menu->addMenu(m_menu);

    show();
    showTool();

#ifdef OCR
    connect(m_tool, &Tool::ocr, this, &TopWidget::ocrStart);
    connect(OcrInstance, &Ocr::ocrEnd, this, &TopWidget::ocrEnd);

    m_widget = new QWidget(this);
    m_widget->setWindowFlag(Qt::FramelessWindowHint, true);
    m_widget->setWindowFlag(Qt::Dialog, true);
    m_widget->setFixedHeight(80);
    m_widget->installEventFilter(this);

    QVBoxLayout *layout = new QVBoxLayout(m_widget);
    layout->setMargin(2);
    layout->setSpacing(2);
    m_text = new QTextEdit(m_widget);
    m_text->document()->setDocumentMargin(0);
    layout->addWidget(m_text);

    QHBoxLayout *btns = new QHBoxLayout;
    btns->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    QPushButton *button = new QPushButton(m_widget);
    button->setFixedSize(24, 24);
    button->setIcon(QIcon(":/images/save.png"));
    button->setToolTip("复制");
    connect(button, &QPushButton::clicked, this, &TopWidget::copyText);
    btns->addWidget(button);

    m_button = new QPushButton(m_widget);
    m_button->setFixedSize(24, 24);
    m_button->setIcon(QIcon(":/images/pencil.png"));
    m_button->setToolTip("编辑");
    connect(m_button, &QPushButton::clicked, this, &TopWidget::editText);
    btns->addWidget(m_button);
    layout->addLayout(btns);
#endif
}

bool TopWidget::contains(const QPoint &point) {
    if (m_path.elementCount() < 2) {
        return rect().contains(point);
    } else {
        return m_path.contains(point);
    }
}

#ifdef OCR
void TopWidget::hideWidget() {
    if (m_widget) {
        m_widget->hide();
        m_text->setReadOnly(true);
        m_text->setStatusTip("");
        m_text->clear();
    }
}
#endif
