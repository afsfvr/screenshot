#include "TopWidget.h"

#include <QTimer>

#ifdef Q_OS_LINUX
#include <X11/Xlib.h>
#include <QX11Info>
#endif

TopWidget::TopWidget(QImage &&image, QVector<Shape *> &vector, const QRect &rect, QMenu *menu) {
    m_image = std::move(image);
    m_vector = std::move(vector);
    tray_menu = menu;
    setGeometry(rect);
    init();
}

TopWidget::TopWidget(QImage &image, QPainterPath &&path, QVector<Shape *> &vector, const QRect &rect, QMenu *menu) {
    m_image = std::move(image);
    m_path = std::move(path);
    m_vector = std::move(vector);
    tray_menu = menu;
    setGeometry(rect);
    init();
}

TopWidget::~TopWidget() {
    delete m_menu;
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

    drawTips(painter);
}

void TopWidget::hideEvent(QHideEvent *event) {
    m_tool->hide();
    BaseWindow::hideEvent(event);
}

void TopWidget::mousePressEvent(QMouseEvent *event) {
    if (event->buttons() == Qt::LeftButton) {
        m_press = true;
        m_point = event->globalPos() - geometry().topLeft();
        if (m_tool->isDraw()) {
            setCursorShape(Qt::BitmapCursor);
            setShape(event->pos());
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
        move(event->globalPos() - m_point);
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
    m_menu->setTitle(text);
    m_menu->addAction("显示", this, &TopWidget::moveTop);
    m_menu->addAction("关闭", this, &TopWidget::close);
    tray_menu->addMenu(m_menu);

    show();
    showTool();
}

bool TopWidget::contains(const QPoint &point) {
    if (m_path.elementCount() < 2) {
        return rect().contains(point);
    } else {
        return m_path.contains(point);
    }
}
