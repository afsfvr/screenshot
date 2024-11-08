#include "mainwindow.h"
#include "TopWidget.h"
#include "GifWidget.h"
#include <QShortcut>
#include <QMessageBox>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent): BaseWindow(parent) {
    m_menu = new QMenu(this);
    m_menu->addAction("截图", this, &MainWindow::start);
    m_menu->addAction("录制GIF", this, [=](){
        m_gif = true;
        start();
    });
    m_menu->addAction("退出", this, &MainWindow::quit);
    m_tray = new QSystemTrayIcon(this);
    m_tray->setContextMenu(m_menu);
    m_tray->setIcon(QIcon(":/images/screenshot.png"));
    m_tray->setToolTip("截图");
    m_tray->show();

    m_state = State::Null;
    m_gif = false;

#ifdef Q_OS_LINUX
    m_monitor = new KeyMouseEvent;
    m_monitor->start();
    m_monitor->resume();
    connect(m_monitor, &KeyMouseEvent::keyPress, this, [=](int code, Qt::KeyboardModifiers modifiers){
        if (! this->isVisible() && code == 38 && modifiers == (Qt::ControlModifier | Qt::AltModifier)) {
            start();
        }
    });
#endif
#ifdef Q_OS_WINDOWS
    if (! RegisterHotKey((HWND)this->winId(), 1, MOD_ALT | MOD_CONTROL, 'A')) {
        QMessageBox::warning(this, "注册热键失败", "注册热键失败");
    }
#endif
    setMouseTracking(true);

    connect(m_tool, &Tool::clickTop, this, &MainWindow::top);
}

MainWindow::~MainWindow() {
#ifdef Q_OS_LINUX
    delete m_monitor;
#endif
#ifdef Q_OS_WINDOWS
    UnregisterHotKey((HWND)this->winId(), 1);
#endif
}

void MainWindow::mousePressEvent(QMouseEvent *event) {
    if (event->buttons() == event->button()) {
        m_press = true;
        m_point = event->pos();
    }
    if (m_state == State::Null) {
        clearDraw();
        m_rect = QRect{};
        m_path.clear();
        if (event->button() == Qt::LeftButton) {
            setCursor(Qt::CrossCursor);
            m_state = State::RectScreen;
        } else if (! m_gif && event->button() == Qt::RightButton) {
            setCursor(Qt::CrossCursor);
            m_state = State::FreeScreen;
            m_path.moveTo(event->pos());
        }
        m_tool->hide();
    } else if (m_state & State::Edit) {
        if (contains(event->pos())) {
            if (event->button() == Qt::LeftButton) {
                if (m_tool->isDraw()) {
                    setCursor(QCursor(QPixmap(":/images/pencil.png"), 0, 24));
                    if (m_shape != nullptr) {
                        qWarning() << "error" << m_shape;
                    }
                    m_shape = m_tool->getShape(event->pos());
                } else {
                    setCursor(Qt::SizeAllCursor);
                    m_tool->hide();
                }
            }
        } else {
            if (event->button() == Qt::LeftButton) {
                setCursor(Qt::CrossCursor);
                clearDraw();
                m_state = State::RectScreen;
            } else if (! m_gif && event->button() == Qt::RightButton) {
                setCursor(Qt::CrossCursor);
                clearDraw();
                m_state = State::FreeScreen;
                m_path.clear();
                m_path.moveTo(event->pos());
            }
            m_tool->hide();
        }
    }
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        if (m_state == State::RectScreen) {
            if (event->pos() == m_point) {
#if defined(Q_OS_WINDOWS)
                if (m_index >= 0 && m_index < m_windows.size()) {
                    m_rect = m_windows[m_index];
                }
#else
                m_rect = geometry();
#endif
            } else {
                m_rect = getRect(m_point, event->pos());
            }
            m_state = State::RectEdit;
            showTool();
            repaint();
        } else if (m_state & State::Edit) {
            if (cursor().shape() == Qt::BitmapCursor) {
                if (m_shape == nullptr) {
                    qWarning() << "error: shape is null";
                } else if (! m_shape->isNull()){
                    m_vector.push_back(m_shape);
                    m_shape = nullptr;
                    clearStack();
                } else {
                    safeDelete(m_shape);
                }
            }
            repaint();
            showTool();
        }
    } else if (event->button() == Qt::RightButton) {
        if (m_state == State::FreeScreen && m_path.elementCount() > 2) {
            m_state = State::FreeEdit;
            m_path.closeSubpath();
            showTool();
            repaint();
        }
    }
    if (event->buttons() == Qt::NoButton) {
        m_press = false;
        unsetCursor();
        if ((event->x() == m_point.x() || event->y() == m_point.y()) && event->button() == Qt::RightButton) {
            if (m_state != State::Null && isValid()) {
                clearDraw();
                m_state = State::Null;
                m_tool->hide();
                repaint();
            } else {
                end();
            }
        }
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent *event) {
    if (m_state == State::RectScreen && (event->buttons() & Qt::LeftButton)) {
        m_rect = getRect(m_point, event->pos());
        repaint();
    } else if (m_state == State::FreeScreen && (event->buttons() & Qt::RightButton)) {
        m_path.lineTo(event->pos());
        repaint();
    } else if ((m_state & State::Edit) && (event->buttons() & Qt::LeftButton)) {
        auto shape =  cursor().shape();
        if (shape == Qt::SizeAllCursor) {
            QPoint point = event->pos() - m_point;
            m_point = event->pos();
            QRect rect = (m_state & State::Free) ? m_path.boundingRect().toRect() : m_rect;
            if (point.x() + rect.left() < 0 || point.x() + rect.right() > width()) {
                point.setX(0);
            }
            if (point.y() + rect.top() < 0 || point.y() + rect.bottom() > height()) {
                point.setY(0);
            }
            if (m_state & State::Free) {
                m_path.translate(point);
            } else {
                m_rect.translate(point);
            }
            for (auto iter = m_vector.begin(); iter != m_vector.end(); ++iter) {
                (*iter)->translate(point);
            }
            for (auto iter = m_stack.begin(); iter != m_stack.end(); ++iter) {
                (*iter)->translate(point);
            }
            repaint();
        } else if (shape == Qt::BitmapCursor) {
            if (m_shape == nullptr) {
                qWarning() << "error: shape is null";
            } else if (contains(event->pos())) {
                m_shape->addPoint(event->pos());
            }
            repaint();
        }
    } else if (event->buttons() == Qt::NoButton) {
        if (m_state == State::Null) {
#ifdef Q_OS_WINDOWS
            for (int i = 0; i < m_windows.size(); ++i) {
                if (m_windows[i].contains(event->pos())) {
                    if (i != m_index) {
                        m_index = i;
                    }
                    break;
                }
            }
#endif
            repaint();
        }
    }
}

void MainWindow::paintEvent(QPaintEvent *event) {
    BaseWindow::paintEvent(event);
    if (m_image.isNull() || m_gray_image.isNull()) {
        m_tool->hide();
        this->hide();
        return;
    }
    QRect rect;
    QPainter painter(this);
    painter.setPen(QPen(Qt::red, 2));
    painter.drawImage(geometry(), m_gray_image);
    if (m_state & State::Free) {
        rect = m_path.boundingRect().toRect();
        painter.drawPath(m_path);
        painter.fillPath(m_path, m_image);
        painter.setClipPath(m_path);
    } else if (m_state & State::Rect) {
        rect = m_rect;
        painter.drawImage(m_rect, m_image, m_rect);
        painter.drawRect(m_rect.adjusted(- 1, - 1, 1, 1));
    } else {
#if defined(Q_OS_WINDOWS)
        if (m_index >= 0 && m_index < m_windows.size()) {
            rect = m_windows[m_index];
            painter.drawImage(rect, m_image, rect);
            painter.drawRect(rect);
        }
#endif
    }
    for (auto iter = m_vector.cbegin(); iter != m_vector.cend(); ++iter) {
        (*iter)->draw(painter);
    }
    if (m_shape != nullptr) {
        m_shape->draw(painter);
    }

    painter.setClipping(false);

    if (m_state == State::Null || (m_state & State::Screen)) {
        const QPoint &cursor = QCursor::pos();
        QPoint point;
        if (cursor.x() + 85 + 10 <= this->width()) {
            point.setX(cursor.x() + 10);
        } else {
            point.setX(cursor.x() - 85 - 10);
        }
        if (cursor.y() + 110 + 25 <= this->height()) {
            point.setY(cursor.y() + 25);
        } else {
            point.setY(cursor.y() - 110 - 20);
        }
        painter.drawRect(point.x() - 1, point.y() - 1, 84 + 2, 84 + 2);
        painter.drawImage(QRect(point.x(), point.y(), 84, 84), m_image, QRect(cursor.x() - 10, cursor.y() - 10, 21, 21));
        painter.drawLine(point.x(), point.y() + 42, point.x() + 84, point.y() + 42);
        painter.drawLine(point.x() + 42, point.y(), point.x() + 42, point.y() + 84);
        QColor color = m_image.pixelColor(cursor);
        QFont font = painter.font();
        font.setPixelSize(13);
        painter.setFont(font);
        painter.drawText(point.x(), point.y() + 97, QString("RGB: %1").arg(color.name().toUpper()));
        painter.drawText(point.x(), point.y() + 110, QString("(%1 x %2)").arg(rect.width()).arg(rect.height()));
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    m_tool->close();
    BaseWindow::closeEvent(event);
}

#ifdef Q_OS_WINDOWS
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, long *result) {
    if (! this->isVisible() && eventType == "windows_generic_MSG") {
        MSG *msg = static_cast<MSG *>(message);
        if (msg->message == WM_HOTKEY && msg->wParam == 1) {
            start();
            return true;
        }
    }
    return QWidget::nativeEvent(eventType, message, result);
}
#endif

void MainWindow::start() {
#ifdef Q_OS_WINDOWS
    updateWindows();
#endif
    disconnect(SIGNAL(choosePath()));
    if (m_gif) {
        connect(this, SIGNAL(choosePath()), this, SLOT(save()), static_cast<Qt::ConnectionType>(Qt::AutoConnection | Qt::UniqueConnection));
    } else {
        connect(this, SIGNAL(choosePath()), m_tool, SLOT(choosePath()), static_cast<Qt::ConnectionType>(Qt::AutoConnection | Qt::UniqueConnection));
    }
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    m_image = fullScreenshot();
    m_gray_image = QImage(m_image.size(), QImage::Format_ARGB32);
    for (int x = 0; x < m_image.width(); ++x) {
        for (int y = 0; y < m_image.height(); ++y) {
            QRgb rgb = m_image.pixel(x, y);
            int r = 0.4 * qRed(rgb);
            int g = 0.4 * qGreen(rgb);
            int b = 0.4 * qBlue(rgb);
            m_gray_image.setPixel(x, y, qRgb(r, g, b));
        }
    }
    setGeometry(0, 0, m_image.width(), m_image.height());
    setWindowState((windowState() & ~(Qt::WindowMinimized | Qt::WindowMaximized)) | Qt::WindowFullScreen);
    setCursor(Qt::CrossCursor);
    setVisible(true);
    m_state = State::Null;
    m_path.clear();
    m_press = false;
    activateWindow();
    repaint();
}

void MainWindow::showTool() {
    if (! this->isVisible()) return;
    QPoint point;
    QRect rect;
    if (m_state & State::Free) {
        rect = m_path.boundingRect().toRect();
    } else {
        rect = m_rect;
    }

    if (rect.bottom() + m_tool->height() + 2 <= this->height()) {
        if (rect.right() + 2 >= m_tool->width()) {
            point.setX(rect.right() - m_tool->width() + 2);
        } else {
            point.setX(rect.right() + 2);
        }
        point.setY(rect.bottom() + 2);
    } else if (rect.top() >= m_tool->height() + 2) {
        if (rect.right() >= m_tool->width()) {
            point.setX(rect.right() - m_tool->width() + 2);
        } else {
            point.setX(rect.right() + 2);
        }
        point.setY(rect.top() - m_tool->height() - 2);
    } else {
        if (rect.right() >= m_tool->width()) {
            point.setX(rect.right() - m_tool->width());
        } else {
            point.setX(rect.right());
        }
        point.setY(rect.bottom() - m_tool->height());
    }
    m_tool->show();
    m_tool->move(point);
}

bool MainWindow::isValid() {
    if (m_state & State::Rect) {
        return m_rect.isValid();
    } else if (m_state & State::Free) {
        return m_path.elementCount() > 2;
#ifdef Q_OS_WINDOWS
    } else {
        if (m_index >= 0 && m_index < m_windows.size()) {
            return m_windows[m_index].isValid();
        }
#endif
    }
    return false;
}

void MainWindow::quit() {
    m_tray->hide();
    this->close();
    QApplication::quit();
}

void MainWindow::save(const QString &path) {
    if (m_gif) {
        if (m_rect.isValid()) {
            new GifWidget{size(), m_rect, m_menu};
        }
    } else {
        QImage image;
        QPainter painter;
        if (m_state & State::Free) {
            QRect rect = m_path.boundingRect().toRect();
            if (rect.width() <= 0 || rect.height() <= 0) return;
            image = QImage(rect.size(), QImage::Format_ARGB32);
            painter.begin(&image);
            painter.fillRect(image.rect(), QColor(0, 0, 0, 0));
            painter.translate(- rect.topLeft());
            painter.fillPath(m_path, m_image);
            painter.setClipPath(m_path);
        } else if (m_state & State::Rect) {
            if (m_rect.width() <= 0 || m_rect.height() <= 0) return;
            image = m_image.copy(m_rect);
            painter.begin(&image);
            painter.translate(- m_rect.topLeft());
        } else {
            return;
        }
        if (! m_vector.empty() || (m_shape != nullptr && ! m_shape->isNull())) {
            for (auto iter = m_vector.begin(); iter != m_vector.end(); ++iter) {
                (*iter)->draw(painter);
            }
            if (m_shape != nullptr && ! m_shape->isNull()) {
                m_shape->draw(painter);
            }
        }
        painter.end();
        if (path.length() == 0) {
            QClipboard *clipboard = QApplication::clipboard();
            if (clipboard) {
                clipboard->setImage(image);
            }
            qInfo() << QString("保存图片到剪贴板%1").arg(clipboard ? "成功" : "失败");
        } else {
            qInfo() << QString("保存图片到%1%2").arg(path, image.save(path) ? "成功" : "失败");
        }
    }
    end();
}

void MainWindow::end() {
    setWindowState((windowState() & ~(Qt::WindowMinimized | Qt::WindowMaximized | Qt::WindowFullScreen)));
    m_state = State::Null;
    m_path.clear();
    m_image = m_gray_image = QImage();
    m_gif = false;
    clearDraw();
    safeDelete(m_shape);
    resize(1, 1);
    repaint();
}

void MainWindow::top() {
    if (m_state & State::Free) {
        QRect rect = m_path.boundingRect().toRect();
        if (rect.width() <= 0 || rect.height() <= 0) return;
        QPoint point = rect.topLeft();
        for (auto iter = m_vector.cbegin(); iter != m_vector.cend(); ++iter) {
            (*iter)->translate(- point);
        }
        QImage image = QImage(rect.size(), QImage::Format_ARGB32);
        QPainter painter(&image);
        painter.fillRect(rect, QColor(0, 0, 0, 0));
        painter.translate(- point);
        painter.fillPath(m_path, m_image);
        painter.end();
        new TopWidget(image, m_path.translated(- point), m_vector, rect, m_menu);
        end();
    } else if (m_state & State::Rect) {
        if (m_rect.width() <= 0 || m_rect.height() <= 0) return;
        QPoint point = m_rect.topLeft();
        for (auto iter = m_vector.cbegin(); iter != m_vector.cend(); ++iter) {
            (*iter)->translate(- point);
        }
        new TopWidget(m_image.copy(m_rect), m_vector, m_rect, m_menu);
        end();
    }
}

bool MainWindow::contains(const QPoint &point) {
    if (m_state & State::Rect) {
        return m_rect.contains(point);
    } else if (m_state & State::Free) {
        return m_path.contains(point);
    }
    return false;
}

#ifdef Q_OS_WINDOWS
void MainWindow::updateWindows() {
    m_windows.clear();
    m_index = 0;

    HWND hwnd = GetTopWindow(nullptr);
    addRect(hwnd);

    while ((hwnd = GetNextWindow(hwnd, GW_HWNDNEXT)) != nullptr) {
        addRect(hwnd);
    }


    const QPoint &point = QCursor::pos();
    for (int i = 0; i < m_windows.size(); ++i) {
        if (m_windows[i].contains(point)) {
            m_index = i;
            break;
        }
    }
}

void MainWindow::addRect(HWND hwnd) {
    if (IsWindow(hwnd) && IsWindowVisible(hwnd)) {
        QRect qrect;
        RECT rect;

        if (DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, (void*)&rect, sizeof(rect)) == S_OK) {
            if (rect.bottom > rect.top && rect.right > rect.left) {
                qrect.setTop(rect.top);
                qrect.setLeft(rect.left);
                qrect.setRight(rect.right);
                qrect.setBottom(rect.bottom);
            }
        }
        if (! qrect.isValid() && GetClientRect(hwnd, &rect)) {
            if (rect.bottom > rect.top && rect.right > rect.left) {
                POINT topLeft = { rect.top, rect.left };
                if (ClientToScreen(hwnd, &topLeft)) {
                    qrect.setTop(topLeft.y);
                    qrect.setLeft(topLeft.x);
                    qrect.setRight(topLeft.x + rect.right - rect.left);
                    qrect.setBottom(topLeft.y + rect.bottom - rect.top);
                }
            }
        }

        if (! qrect.isValid() && GetWindowRect(hwnd, &rect)) {
            if (rect.bottom > rect.top && rect.right > rect.left) {
                qrect.setTop(rect.top);
                qrect.setLeft(rect.left);
                qrect.setRight(rect.right);
                qrect.setBottom(rect.bottom);
            }
        }

        if (qrect.isValid()) {
            m_windows.push_back(qrect);
        }
    }
}

#endif
