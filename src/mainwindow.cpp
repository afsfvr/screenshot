#include "mainwindow.h"
#include "TopWidget.h"
#include "GifWidget.h"
#include "LongWidget.h"

#include <QShortcut>
#include <QMessageBox>
#include <QTimer>
#include <QStandardPaths>
#include <assert.h>

MainWindow *MainWindow::self = nullptr;
MainWindow *MainWindow::instance() {
    return MainWindow::self;
}

MainWindow::MainWindow(QWidget *parent): BaseWindow(parent),
    m_state{State::Null}, m_resize{ResizeImage::NoResize}, m_gif{false}, m_setting{new SettingWidget} {

    assert(MainWindow::self == nullptr);
    MainWindow::self = this;
    initTray();
#ifdef Q_OS_LINUX
    m_monitor = new KeyMouseEvent;
    m_monitor->start();
    m_monitor->resume();
    connect(m_monitor, &KeyMouseEvent::keyPress, this, &MainWindow::keyPress);
    connect(m_monitor, &KeyMouseEvent::mouseWheel, this, &MainWindow::mouseWheel);
    connect(this, &MainWindow::started, this, &MainWindow::grabMouseEvent);
#endif
#ifdef OCR
    connect(m_tool, &Tool::ocr, this, &MainWindow::ocrStart);
#endif
    setMouseTracking(true);

    m_tool->setInMainWindow(true);
    connect(m_tool, &Tool::clickTop, this, &MainWindow::top);
    connect(m_tool, &Tool::longScreenshot, this, &MainWindow::longScreenshot);
    connect(m_setting, &SettingWidget::autoSaveChanged, this, &MainWindow::updateAutoSave);
    connect(m_setting, &SettingWidget::captureChanged, this, &MainWindow::updateCapture);
    connect(m_setting, &SettingWidget::recordChanged, this, &MainWindow::updateRecord);
    QTimer::singleShot(500, this, [=](){ m_setting->readConfig(); });
}

MainWindow::~MainWindow() {
    MainWindow::self = nullptr;
    safeDelete(m_setting);

#ifdef Q_OS_LINUX
    delete m_monitor;
#endif
#ifdef Q_OS_WINDOWS
    UnregisterHotKey((HWND)this->winId(), 1);
    UnregisterHotKey((HWND)this->winId(), 2);
    UnregisterHotKey((HWND)this->winId(), 3);
#endif
}

void MainWindow::connectTopWidget(TopWidget *t) {
    connect(m_setting, &SettingWidget::scaleKeyChanged, t, &TopWidget::scaleKeyChanged);
    t->scaleKeyChanged(m_setting->scaleCtrl());
#if defined(Q_OS_LINUX)
    connect(m_monitor, &KeyMouseEvent::mouseRelease, t, &TopWidget::mouseRelease);
#endif
}

void MainWindow::mousePressEvent(QMouseEvent *event) {
    m_mouse_pos = event->pos();
#ifdef Q_OS_LINUX
    if (m_grab_mouse) {
        this->releaseMouse();
        m_grab_mouse = false;
    }
#endif
    if (event->buttons() == event->button()) {
        m_press = true;
        m_point = m_mouse_pos;
    }
    if (m_state == State::Null) {
        clearDraw();
        m_rect = QRect{};
        m_path.clear();
        if (event->button() == Qt::LeftButton) {
            m_state = State::RectScreen;
        } else if (! m_gif && event->button() == Qt::RightButton) {
            m_state = State::FreeScreen;
            m_path.moveTo(m_mouse_pos);
        }
        m_tool->hide();
    } else if (m_state & State::Edit) {
        if (m_resize != ResizeImage::NoResize) {
            clearDraw();
            m_tool->hide();
            update();
        } else if (contains(m_mouse_pos)) {
            if (event->button() == Qt::LeftButton) {
                if (m_tool->isDraw()) {
                    setShape(m_mouse_pos);
                } else {
                    m_tool->hide();
                    if (m_move_shape) {
                        m_move_shape->moveEnd();
                        m_move_shape = nullptr;
                    }
                    for (auto iter = m_vector.begin(); iter != m_vector.end(); ++iter) {
                        if ((*iter)->canMove(m_mouse_pos)) {
                            m_move_shape = *iter;
                            break;
                        }
                    }
                }
            }
        } else {
            if (event->button() == Qt::LeftButton) {
                clearDraw();
                m_rect = QRect();
                m_state = State::RectScreen;
            } else if (! m_gif && event->button() == Qt::RightButton) {
                clearDraw();
                m_state = State::FreeScreen;
                m_path.clear();
                m_path.moveTo(m_mouse_pos);
            }
            m_tool->hide();
        }
    }
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event) {
    m_mouse_pos = event->pos();
    if (event->button() == Qt::LeftButton) {
        if (m_state == State::RectScreen) {
            m_rect = getRect(m_point, event->pos());
            if (m_rect.width() <= 1 || m_rect.height() <= 1) {
                if (m_index >= 0 && m_index < m_windows.size()) {
                    m_rect = m_windows[m_index];
                    const QRect &rect = this->geometry();
                    if (m_rect.top() < rect.top()) {
                        m_rect.setTop(rect.top());
                    }
                    if (m_rect.left() < rect.left()) {
                        m_rect.setLeft(rect.left());
                    }
                    if (m_rect.right() > rect.right()) {
                        m_rect.setRight(rect.right());
                    }
                    if (m_rect.bottom() > rect.bottom()) {
                        m_rect.setBottom(rect.bottom());
                    }
                }
            }
            if (m_rect.width() <= 0 || m_rect.height() <= 0) {
                m_state = State::Null;
            } else {
                m_state = State::RectEdit;
                showTool();
            }
            update();
        } else if (m_state & State::Edit) {
            if (m_cursor == Qt::BitmapCursor) {
                if (! contains(event->pos())) {
                    setCursorShape();
                }
                if (m_shape == nullptr) {
                } else if (! m_shape->isNull()){
                    m_vector.push_back(m_shape);
                    m_shape = nullptr;
                    clearStack();
                } else {
                    safeDelete(m_shape);
                }
            }
            m_resize = ResizeImage::NoResize;
            update();
            showTool();
        }
        if (m_move_shape) {
            m_move_shape->moveEnd();
            m_move_shape = nullptr;
        }
    } else if (event->button() == Qt::RightButton) {
        if (m_state == State::FreeScreen && m_path.elementCount() > 1) {
            m_state = State::FreeEdit;
            m_path.closeSubpath();
            showTool();
            update();
        }
    }
    if (event->buttons() == Qt::NoButton) {
        m_press = false;
        QPoint p = event->pos();
        if ((p.x() == m_point.x() || p.y() == m_point.y()) && event->button() == Qt::RightButton) {
            if (m_state != State::Null && isValid()) {
                setCursorShape(Qt::CrossCursor);
                clearDraw();
                m_state = State::Null;
                m_tool->hide();
                update();
            } else {
                setCursorShape();
                end();
            }
        }
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent *event) {
    m_mouse_pos = event->pos();
    if (m_state == State::RectScreen && (event->buttons() & Qt::LeftButton)) {
        m_rect = getRect(m_point, event->pos());
        update();
    } else if (m_state == State::FreeScreen && (event->buttons() & Qt::RightButton)) {
        m_path.lineTo(event->pos());
        update();
    } else if ((m_state & State::Edit) && (event->buttons() & Qt::LeftButton)) {
        if (m_resize != ResizeImage::NoResize) {
            QPoint point = event->pos();
            if (m_resize & ResizeImage::Top) {
                if (point.y() > m_rect.bottom()) {
                    m_resize &= ~ResizeImage::Top;
                    m_resize |= ResizeImage::Bottom;
                    m_rect.setTop(m_rect.bottom());
                    m_rect.setBottom(point.y());
                } else {
                    m_rect.setTop(point.y());
                }
            } else if (m_resize & ResizeImage::Bottom) {
                if (point.y() < m_rect.top()) {
                    m_resize &= ~ResizeImage::Bottom;
                    m_resize |= ResizeImage::Top;
                    m_rect.setBottom(m_rect.top());
                    m_rect.setTop(point.y());
                } else {
                    m_rect.setBottom(point.y());
                }
            }
            if (m_resize & ResizeImage::Left) {
                if (point.x() > m_rect.right()) {
                    m_resize &= ~ResizeImage::Left;
                    m_resize |= ResizeImage::Right;
                    m_rect.setLeft(m_rect.right());
                    m_rect.setRight(point.x());
                } else {
                    m_rect.setLeft(point.x());
                }
            } else if (m_resize & ResizeImage::Right) {
                if (point.x() < m_rect.left()) {
                    m_resize &= ~ResizeImage::Right;
                    m_resize |= ResizeImage::Left;
                    m_rect.setRight(m_rect.left());
                    m_rect.setLeft(point.x());
                } else {
                    m_rect.setRight(point.x());
                }
            }
            update();
        } else if (m_cursor == Qt::SizeAllCursor) {
            if (m_move_shape) {
                m_move_shape->movePoint(m_mouse_pos);
            } else {
                QPoint point = event->pos() - m_point;
                m_point = event->pos();
                const QRect &rect = getGeometry();
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
            }
            update();
        } else if (m_cursor == Qt::BitmapCursor) {
            QRect &&rect = getGeometry();
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
            update();
        }
    } else if (event->buttons() == Qt::NoButton) {
        if (m_state == State::Null) {
            setCursorShape(Qt::CrossCursor);
            for (int i = 0; i < m_windows.size(); ++i) {
                if (m_windows[i].contains(event->pos())) {
                    if (i != m_index) {
                        m_index = i;
                    }
                    break;
                }
            }
            update();
        } else if (m_state == State::RectEdit) {
            m_resize = ResizeImage::NoResize;
            QPoint point = event->pos();
            if (m_rect.contains(point)) {
                if (m_tool->isDraw()) {
                    setCursorShape(Qt::BitmapCursor);
                } else {
                    setCursorShape(Qt::SizeAllCursor);
                }
            } else {
                if (m_rect.left() - point.x() >= 0 && m_rect.left() - point.x() <= 3) {
                    m_resize |= ResizeImage::Left;
                } else if (point.x() - m_rect.right() >= 0 && point.x() - m_rect.right() <= 3) {
                    m_resize |= ResizeImage::Right;
                }
                if (m_rect.top() - point.y() >= 0 && m_rect.top() - point.y() <= 3) {
                    m_resize |= ResizeImage::Top;
                } else if (point.y() - m_rect.bottom() >= 0 && point.y() - m_rect.bottom() <= 3) {
                    m_resize |= ResizeImage::Bottom;
                }
                if (m_resize == ResizeImage::NoResize) {
                    setCursorShape(Qt::CrossCursor);
                } else {
                    if (m_resize == ResizeImage::TopLeft || m_resize == ResizeImage::BottomRight) {
                        setCursorShape(Qt::SizeFDiagCursor);
                    } else if (m_resize == ResizeImage::TopRight || m_resize == ResizeImage::BottomLeft) {
                        setCursorShape(Qt::SizeBDiagCursor);
                    } else if (m_resize == ResizeImage::Top || m_resize == ResizeImage::Bottom) {
                        setCursorShape(Qt::SizeVerCursor);
                    } else if (m_resize == ResizeImage::Left || m_resize == ResizeImage::Right) {
                        setCursorShape(Qt::SizeHorCursor);
                    } else {
                        addTip("unknown resize");
                        qWarning() << "unknown resize";
                        setCursorShape();
                    }
                }
            }
        } else if (m_state == State::FreeEdit) {
            if (m_path.contains(event->pos())) {
                if (m_tool->isDraw()) {
                    setCursorShape(Qt::BitmapCursor);
                } else {
                    setCursorShape(Qt::SizeAllCursor);
                }
            } else {
                setCursorShape(Qt::CrossCursor);
            }
        }
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    BaseWindow::keyPressEvent(event);
    if (event->modifiers() == Qt::NoModifier) {
        switch (event->key()) {
        case Qt::Key_Left:
            if (m_state & State::Free) {
                if (m_path.boundingRect().left() > 0) {
                    m_path.translate(-1, 0);
                    update();
                    showTool();
                }
            } else if (m_state & State::Rect) {
                if (m_rect.left() > 0) {
                    m_rect.translate(-1, 0);
                    update();
                    showTool();
                }
            }
            break;
        case Qt::Key_Right:
            if (m_state & State::Free) {
                if (m_path.boundingRect().right() * m_ratio < m_image.width()) {
                    m_path.translate(1, 0);
                    update();
                    showTool();
                }
            } else if (m_state & State::Rect) {
                if (m_rect.right() * m_ratio < m_image.width()) {
                    m_rect.translate(1, 0);
                    update();
                    showTool();
                }
            }
            break;
        case Qt::Key_Up:
            if (m_state & State::Free) {
                if (m_path.boundingRect().top() > 0) {
                    m_path.translate(0, -1);
                    update();
                    showTool();
                }
            } else if (m_state & State::Rect) {
                if (m_rect.top() > 0) {
                    m_rect.translate(0, -1);
                    update();
                    showTool();
                }
            }
            break;
        case Qt::Key_Down:
            if (m_state & State::Free) {
                if (m_path.boundingRect().bottom() * m_ratio < m_image.height()) {
                    m_path.translate(0, 1);
                    update();
                    showTool();
                }
            } else if (m_state & State::Rect) {
                if (m_rect.bottom() * m_ratio < m_image.height()) {
                    m_rect.translate(0, 1);
                    update();
                    showTool();
                }
            }
            break;
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
        QBrush brush{m_image};
        QTransform transform;
        transform.scale(1.0 / m_ratio, 1.0 / m_ratio);
        brush.setTransform(transform);
        painter.fillPath(m_path, brush);
        painter.setClipPath(m_path);
    } else if (m_state & State::Rect) {
        rect = m_rect;
        QRectF sourceRect = QRectF(rect.left() * m_ratio,
                                   rect.top() * m_ratio,
                                   rect.width() * m_ratio,
                                   rect.height() * m_ratio);
        painter.drawImage(rect, m_image, sourceRect);
        painter.drawRect(rect.adjusted(- 1, - 1, 1, 1));
        painter.setClipRect(rect);
    } else {
        if (m_index >= 0 && m_index < m_windows.size()) {
            rect = m_windows[m_index];
            QRectF sourceRect = QRectF(rect.left() * m_ratio,
                                       rect.top() * m_ratio,
                                       rect.width() * m_ratio,
                                       rect.height() * m_ratio);
            painter.drawImage(rect, m_image, sourceRect);
            painter.drawRect(rect);
            painter.setClipRect(rect);
        }
    }
    for (auto iter = m_vector.cbegin(); iter != m_vector.cend(); ++iter) {
        (*iter)->draw(painter);
    }
    if (m_shape != nullptr) {
        m_shape->draw(painter);
    }

    painter.setClipping(false);
    if (m_state == State::Null || (m_state & State::Screen) || (m_press && m_resize != ResizeImage::NoResize)) {
        const QPoint &cursor = m_mouse_pos;
        if (cursor.x() >= 0 && cursor.y() >= 0) {
            QPoint point;
            if (cursor.x() + 85 + 10 <= this->width()) {
                point.setX(cursor.x() + 10);
            } else {
                point.setX(cursor.x() - 85 - 10);
            }
            if (cursor.y() + 140 + 25 <= this->height()) {
                point.setY(cursor.y() + 25);
            } else {
                point.setY(cursor.y() - 140 - 20);
            }
            painter.fillRect(point.x() - 3, point.y() - 3 ,90, 145, QColor(0, 0, 0, 150));
            painter.drawRect(point.x() - 1, point.y() - 1, 84 + 2, 84 + 2);
            painter.drawImage(QRect(point.x(), point.y(), 84, 84), m_image, QRect((cursor.x() - 10) * m_ratio, (cursor.y() - 10) * m_ratio, 21 * m_ratio, 21 * m_ratio));
            painter.drawLine(point.x(), point.y() + 42, point.x() + 84, point.y() + 42);
            painter.drawLine(point.x() + 42, point.y(), point.x() + 42, point.y() + 84);
            QColor color = m_image.pixelColor(cursor * m_ratio);
            QFont font = painter.font();
            font.setPixelSize(13);
            painter.setFont(font);
            painter.drawText(point.x(), point.y() + 97, QString("RGB: %1").arg(color.name().toUpper()));
            painter.drawText(point.x(), point.y() + 111, QString("按C复制颜色"));
            painter.drawText(point.x(), point.y() + 125, QString("%1, %2").arg(cursor.x() * m_ratio).arg(cursor.y() * m_ratio));
            painter.drawText(point.x(), point.y() + 138, QString("(%1 x %2)").arg(rect.width() * m_ratio).arg(rect.height() * m_ratio));
        }
    }

    drawTips(painter);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    m_tool->close();
    BaseWindow::closeEvent(event);
}

#ifdef Q_OS_WINDOWS
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
#else
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, long *result) {
#endif
    if (! this->isVisible() && eventType == "windows_generic_MSG") {
        MSG *msg = reinterpret_cast<MSG*>(message);
        if (msg->message == WM_HOTKEY) {
            if (msg->wParam == 1) {
                saveImage();
                return true;
            } else if (msg->wParam == 2) {
                start();
                return true;
            } else if (msg->wParam == 3) {
                gifStart();
                return true;
            }
        } else if (msg->message == WM_USER + 1024) {
            emit mouseWheeled(msg->wParam);
        }
    }
    return QWidget::nativeEvent(eventType, message, result);
}
#endif

void MainWindow::saveImage() {
    const QString path = m_setting->autoSavePath();
    if (path.length() == 0) {
        qWarning() << "path为空，取消截图";
        m_tray->showMessage("路径错误", "path为空，取消截图", QSystemTrayIcon::Warning, 3000);
        return;
    }

    if (! QDir{path}.mkpath(path)) {
        QString msg = QString("创建文件夹%1失败").arg(path);
        qWarning() << msg;
        m_tray->showMessage("路径错误", msg, QSystemTrayIcon::Warning, 3000);
        return;
    }

    QString windowTitle{"unknown"};
    QImage image;
    if (m_setting->fullScreen()) {
        windowTitle = "全屏";
        image = fullScreenshot();
    } else {
        QRect rect;
#if defined(Q_OS_WINDOWS)
        HWND hwnd = GetForegroundWindow();
        rect = getRectByHwnd(hwnd);
        if (rect.isEmpty()) {
            while ((hwnd = GetNextWindow(hwnd, GW_HWNDNEXT)) != nullptr) {
                rect = getRectByHwnd(hwnd);
                if (rect.isValid()) {
                    break;
                }
            }
        }
        char title[256];
        GetWindowTextA(hwnd, title, 256);
        windowTitle = QString::fromLocal8Bit(title);
#elif defined(Q_OS_LINUX)
        Display *display = XOpenDisplay(nullptr);
        Window rootWindow = DefaultRootWindow(display);

        Atom actual_type;
        int actual_format;
        unsigned long nitems, bytes_after;
        unsigned char *prop = nullptr;
        Atom _NET_ACTIVE_WINDOW = XInternAtom(display, "_NET_ACTIVE_WINDOW", True);

        if (XGetWindowProperty(display, rootWindow, _NET_ACTIVE_WINDOW,
                               0, (~0L), False, AnyPropertyType,
                               &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success && prop) {
            if (nitems > 0) {
                Window active_window = *(Window *)prop;
                XFree(prop);
                XWindowAttributes attributes;
                XGetWindowAttributes(display, active_window, &attributes);
                if ((attributes.map_state == IsViewable) && (attributes.width != attributes.height || attributes.width > 5 || attributes.height > 5)) {
                    rect = {attributes.x, attributes.y, attributes.width, attributes.height};
                    windowTitle = getWindowTitle(display, active_window);
                }
            }
        }
        if (rect.isEmpty()) {
            Window parent;
            Window* children;
            unsigned int nChildren;
            if (XQueryTree(display, rootWindow, &rootWindow, &parent, &children, &nChildren)) {
                for (int i = nChildren - 1; i >= 0; --i) {
                    XWindowAttributes attributes;
                    XGetWindowAttributes(display, children[i], &attributes);

                    if ((attributes.map_state != IsViewable) || (attributes.width == attributes.height && attributes.width <= 5)) {
                        continue;
                    }
                    rect = {attributes.x, attributes.y, attributes.width, attributes.height};
                    windowTitle = getWindowTitle(display, children[i]);
                    break;
                }

                XFree(children);
            }
        }

        XCloseDisplay(display);
#endif
        QList<QScreen*> list = QApplication::screens();
        for (auto iter = list.cbegin(); iter != list.cend(); ++iter) {
            QRect r = (*iter)->geometry();
            r.setWidth(r.width() * (*iter)->devicePixelRatio());
            r.setHeight(r.height() * (*iter)->devicePixelRatio());
            if (r.contains(rect, false)) {
                QScreen *screen = (*iter);
                image = screen->grabWindow(0, rect.x(), rect.y(), rect.width(), rect.height()).toImage();
                break;
            }
        }

        if (image.isNull()) {
            image = fullScreenshot().copy(rect);
        }
    }
    if (image.isNull()) {
        qWarning() << "图片为空";
        m_tray->showMessage("截图失败", "截图失败", QSystemTrayIcon::Critical, 3000);
    } else {
        static QRegularExpression regex(R"([\/:*?"<>|])");
        windowTitle.replace(regex, "_");
        std::string saveFormat = m_setting->saveFormat().toStdString();
        const char *format = saveFormat.c_str();
        QString datetimeString = QDateTime::currentDateTime().toString("yyyy-MM-dd-hh-mm-ss.zzz");
        QString imagePath = QString("%1%2%3_%4.%5").arg(path, QDir::separator(), windowTitle, datetimeString, format);
        bool ret = image.save(imagePath, format);
        if (! ret) {
            imagePath = QString("%1%2%3.%4").arg(path, QDir::separator(), datetimeString, format);
            ret = image.save(imagePath, format);
        }
        if (ret) {
            m_tray->showMessage("截图成功", QString("图片已保存到%1").arg(imagePath), QIcon(QPixmap::fromImage(image)), 3000);
        } else {
            m_tray->showMessage("截图失败", QString("保存图片到%1失败").arg(imagePath), QSystemTrayIcon::Critical, 3000);
        }
    }
}

void MainWindow::start() {
    m_ratio = 1;
    m_mouse_pos = QCursor::pos();
    disconnect(SIGNAL(choosePath()));
    m_tool->setEditShow(! m_gif);
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
    updateWindows();
    setWindowState((windowState() & ~(Qt::WindowMinimized | Qt::WindowMaximized)) | Qt::WindowFullScreen);
    setFixedSize(m_image.size() / m_ratio);
    setGeometry(0, 0, m_image.width() / m_ratio, m_image.height() / m_ratio);
    setCursorShape(Qt::CrossCursor);
    setVisible(true);
    m_state = State::Null;
    m_resize = ResizeImage::NoResize;
    m_path.clear();
    m_press = false;
    activateWindow();
    update();
    emit started();
}

void MainWindow::gifStart() {
    m_gif = true;
    start();
}

void MainWindow::showTool() {
    if (! this->isVisible()) return;
    QPoint point;
    const QRect &rect = getGeometry();

    if (rect.bottom() + m_tool->height() + 2 <= this->height()) {
        if (rect.right() + 2 >= m_tool->width()) {
            point.setX(rect.right() - m_tool->width() + 2);
        } else {
            point.setX(rect.right() + 2);
        }
        point.setY(rect.bottom() + 3);
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
    m_tool->move(getScreenPoint(point));
}

bool MainWindow::isValid() const {
    if (m_state & State::Rect) {
        return m_rect.isValid();
    } else if (m_state & State::Free) {
        return m_path.elementCount() > 2;
    } else {
        if (m_index >= 0 && m_index < m_windows.size()) {
            return m_windows[m_index].isValid();
        }
    }
    return false;
}

QRect MainWindow::getGeometry() const {
    if (m_state & State::Free) {
        return m_path.boundingRect().toRect();
    } else if (m_state & State::Rect) {
        return m_rect;
    } else {
        return {};
    }
}

#ifdef Q_OS_LINUX
void MainWindow::keyPress(int code, Qt::KeyboardModifiers modifiers) {
    if (! this->isVisible()) {
        static const std::unordered_map<int, char> xEventcodeToChar = {
            {24, 'Q'}, {25, 'W'}, {26, 'E'}, {27, 'R'}, {28, 'T'}, {29, 'Y'}, {30, 'U'},
            {31, 'I'}, {32, 'O'}, {33, 'P'}, {38, 'A'}, {39, 'S'}, {40, 'D'}, {41, 'F'},
            {42, 'G'}, {43, 'H'}, {44, 'J'}, {45, 'K'}, {46, 'L'}, {52, 'Z'}, {53, 'X'},
            {54, 'C'}, {55, 'V'}, {56, 'B'}, {57, 'N'}, {58, 'M'}
        };

        std::unordered_map<int, char>::const_iterator it = xEventcodeToChar.find(code);
        if (it == xEventcodeToChar.cend()) {
            return;
        } else {
            code = it->second;
        }
        const HotKey &save = m_setting->autoSave();
        const HotKey &capture = m_setting->capture();
        const HotKey &record = m_setting->record();
        if (save.modifiers != Qt::NoModifier && save.modifiers == modifiers && save.key == static_cast<quint32>(code)) {
            saveImage();
        } else if (capture.modifiers != Qt::NoModifier && capture.modifiers == modifiers && capture.key == static_cast<quint32>(code)) {
            start();
        } else if (record.modifiers != Qt::NoModifier && record.modifiers == modifiers && record.key == static_cast<quint32>(code)) {
            gifStart();
        }
    }
}

void MainWindow::mouseWheel(QSharedPointer<QWheelEvent> event) {
    emit mouseWheeled(event->angleDelta().y() <= 0);
}

void MainWindow::grabMouseEvent() {
    this->grabMouse();
    m_grab_mouse = true;
}
#endif

#ifdef OCR
void MainWindow::ocrStart() {
    auto *t = top();
    if (t) {
        t->ocrStart();
    }
}
#endif

void MainWindow::longScreenshot() {
    if (m_state & State::Rect) {
        if (m_rect.width() <= 0 || m_rect.height() <= 0) return;
        QImage image = m_image.copy(m_rect.left() * m_ratio, m_rect.top() * m_ratio, m_rect.width() * m_ratio, m_rect.height() * m_ratio);
        auto *l = new LongWidget(image, m_rect, size(), m_menu, m_ratio);
        connect(this, &MainWindow::mouseWheeled, l, &LongWidget::mouseWheel);
        end();
    }
}

void MainWindow::updateHotkey() {
    if (m_setting->isMinimized()) {
        m_setting->showNormal();
    } else if (m_setting->isVisible()) {
        m_setting->activateWindow();
        m_setting->raise();
    } else {
        m_setting->show();
    }
}

void MainWindow::updateAutoSave(const HotKey &key, quint8 mode, const QString &path) {
    Q_UNUSED(mode);
    Q_UNUSED(path);
#ifdef Q_OS_WINDOWS
    UnregisterHotKey((HWND)this->winId(), 1);
    quint32 fsModifiers = 0;
#endif
    if (key.modifiers != Qt::NoModifier) {
        QString s{"自动保存("};
        if (key.modifiers & Qt::ControlModifier) {
#ifdef Q_OS_WINDOWS
            fsModifiers |= MOD_CONTROL;
#endif
            s.append("Ctrl+");
        }
        if (key.modifiers & Qt::AltModifier) {
#ifdef Q_OS_WINDOWS
            fsModifiers |= MOD_ALT;
#endif
            s.append("Alt+");
        }
        if (key.modifiers & Qt::ShiftModifier) {
#ifdef Q_OS_WINDOWS
            fsModifiers |= MOD_SHIFT;
#endif
            s.append("Shift+");
        }
#if defined(Q_OS_WINDOWS)
        if (RegisterHotKey((HWND)this->winId(), 1, fsModifiers, key.key)) {
            s.append(static_cast<char>(key.key)).append(")");
            m_action1->setText(s);
        } else {
            QMessageBox::warning(this, "注册热键失败", "自动保存热键注册失败");
            m_setting->cleanAutoSaveKey();
            m_action1->setText("自动保存(未设置)");
        }
#else
        s.append(static_cast<char>(key.key)).append(")");
        m_action1->setText(s);
#endif
    } else {
        m_action1->setText("自动保存(未设置)");
    }
}

void MainWindow::updateCapture(const HotKey &key) {
#ifdef Q_OS_WINDOWS
    UnregisterHotKey((HWND)this->winId(), 2);
    quint32 fsModifiers = 0;
#endif
    if (key.modifiers != Qt::NoModifier) {
        QString s{"截图("};
        if (key.modifiers & Qt::ControlModifier) {
#ifdef Q_OS_WINDOWS
            fsModifiers |= MOD_CONTROL;
#endif
            s.append("Ctrl+");
        }
        if (key.modifiers & Qt::AltModifier) {
#ifdef Q_OS_WINDOWS
            fsModifiers |= MOD_ALT;
#endif
            s.append("Alt+");
        }
        if (key.modifiers & Qt::ShiftModifier) {
#ifdef Q_OS_WINDOWS
            fsModifiers |= MOD_SHIFT;
#endif
            s.append("Shift+");
        }
#if defined(Q_OS_WINDOWS)
        if (RegisterHotKey((HWND)this->winId(), 2, fsModifiers, key.key)) {
            s.append(static_cast<char>(key.key)).append(")");
            m_action2->setText(s);
        } else {
            QMessageBox::warning(this, "注册热键失败", "截图注册热键失败");
            m_setting->cleanCaptureKey();
            m_action2->setText("截图(未设置)");
        }
#else
        s.append(static_cast<char>(key.key)).append(")");
        m_action2->setText(s);
#endif
    } else {
        m_action2->setText("截图(未设置)");
    }
}

void MainWindow::updateRecord(const HotKey &key) {
#ifdef Q_OS_WINDOWS
    UnregisterHotKey((HWND)this->winId(), 3);
    quint32 fsModifiers = 0;
#endif
    if (key.modifiers != Qt::NoModifier) {
        QString s{"录制GIF("};
        if (key.modifiers & Qt::ControlModifier) {
#ifdef Q_OS_WINDOWS
            fsModifiers |= MOD_CONTROL;
#endif
            s.append("Ctrl+");
        }
        if (key.modifiers & Qt::AltModifier) {
#ifdef Q_OS_WINDOWS
            fsModifiers |= MOD_ALT;
#endif
            s.append("Alt+");
        }
        if (key.modifiers & Qt::ShiftModifier) {
#ifdef Q_OS_WINDOWS
            fsModifiers |= MOD_SHIFT;
#endif
            s.append("Shift+");
        }
#if defined(Q_OS_WINDOWS)
        if (RegisterHotKey((HWND)this->winId(), 3, fsModifiers, key.key)) {
            s.append(static_cast<char>(key.key)).append(")");
            m_action3->setText(s);
        } else {
            QMessageBox::warning(this, "注册热键失败", "GIF注册热键失败");
            m_setting->cleanRecordKey();
            m_action3->setText("录制GIF(未设置)");
        }
#else
        s.append(static_cast<char>(key.key)).append(")");
        m_action3->setText(s);
#endif
    } else {
        m_action3->setText("录制GIF(未设置)");
    }
}

void MainWindow::quit() {
    m_tray->hide();
    this->close();
    QApplication::quit();
}

void MainWindow::save(const QString &path) {
    if (m_gif) {
        if (m_rect.isValid()) {
            new GifWidget{size(), m_rect, m_menu, m_ratio};
        }
    } else {
        QImage image;
        QPainter painter;
        if (m_state & State::Free) {
            QRect rect = m_path.boundingRect().toRect();
            if (rect.width() <= 0 || rect.height() <= 0) return;
            image = QImage(rect.size() * m_ratio, QImage::Format_ARGB32);
            painter.begin(&image);
            painter.fillRect(image.rect(), QColor(0, 0, 0, 0));
            painter.translate(- rect.topLeft() * m_ratio);
            QTransform transform;
            transform.scale(m_ratio, m_ratio);
            QPainterPath path = transform.map(m_path);
            painter.fillPath(path, m_image);
            painter.setClipPath(path);
        } else if (m_state & State::Rect) {
            if (m_rect.width() <= 0 || m_rect.height() <= 0) return;
            image = m_image.copy(m_rect.left() * m_ratio, m_rect.top() * m_ratio, m_rect.width() * m_ratio, m_rect.height() * m_ratio);
            painter.begin(&image);
            painter.translate(- m_rect.topLeft() * m_ratio);
        } else {
            return;
        }
        if (! m_vector.empty() || (m_shape != nullptr && ! m_shape->isNull())) {
            for (auto iter = m_vector.begin(); iter != m_vector.end(); ++iter) {
                (*iter)->scale(m_ratio, m_ratio);
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
    m_state = State::Null;
    m_resize = ResizeImage::NoResize;
    m_path.clear();
    m_image = m_gray_image = QImage();
    m_gif = false;
    clearDraw();
    safeDelete(m_shape);
    resize(1, 1);
    update();
    emit finished();
}

TopWidget *MainWindow::top() {
    if (m_state & State::Free) {
        QRect rect = m_path.boundingRect().toRect();
        if (rect.width() <= 0 || rect.height() <= 0) return nullptr;
        QPoint point = rect.topLeft();
        for (auto iter = m_vector.cbegin(); iter != m_vector.cend(); ++iter) {
            (*iter)->translate(- point);
        }
        QImage image = QImage(rect.size() * m_ratio, QImage::Format_ARGB32);
        QPainter painter(&image);
        painter.fillRect(image.rect(), QColor(0, 0, 0, 0));
        painter.translate(- point * m_ratio);
        QTransform transform;
        transform.scale(m_ratio, m_ratio);
        painter.fillPath(transform.map(m_path), m_image);
        painter.end();
        auto *t = new TopWidget(image, m_path.translated(- point), m_vector, rect, m_menu, m_ratio);
        connectTopWidget(t);
        end();
        return t;
    } else if (m_state & State::Rect) {
        if (m_rect.width() <= 0 || m_rect.height() <= 0) return nullptr;
        QPoint point = m_rect.topLeft();
        for (auto iter = m_vector.cbegin(); iter != m_vector.cend(); ++iter) {
            (*iter)->translate(- point);
        }

        QImage image = m_image.copy(m_rect.left() * m_ratio, m_rect.top() * m_ratio, m_rect.width() * m_ratio, m_rect.height() * m_ratio);
        auto *t = new TopWidget(std::move(image), m_vector, m_rect, m_menu, m_ratio);
        connectTopWidget(t);
        end();
        return t;
    }
    return nullptr;
}

void MainWindow::initTray() {
    m_menu = new QMenu(this);
    m_menu->addAction(QIcon(":/images/setting.png"), "设置", this, &MainWindow::updateHotkey);
    m_action1 = m_menu->addAction(QIcon(":/images/save.png"), "自动保存(未设置)", this, &MainWindow::openSaveDir);
    m_action1->setToolTip("点击进入目录");
    m_action2 = m_menu->addAction(QIcon(":/images/screenshot.ico"), "截图(未设置)", this, &MainWindow::start);
    m_action2->setToolTip("点击截图");
    m_action3 = m_menu->addAction(QIcon(":/images/gif.png"), "录制GIF(未设置)", this, &MainWindow::gifStart);
    m_action3->setToolTip("点击录制GIF");
    m_menu->addAction(QIcon(":/images/exit.png"), "退出", this, &MainWindow::quit);
    m_menu->addSeparator();
    m_tray = new QSystemTrayIcon(this);
    m_tray->setContextMenu(m_menu);
    m_tray->setIcon(QIcon(":/images/screenshot.ico"));
    m_tray->setToolTip("截图工具");
    m_tray->show();
    connect(m_tray, &QSystemTrayIcon::messageClicked, this, &MainWindow::openSaveDir);
}

void MainWindow::openSaveDir() {
    QString path = m_setting->autoSavePath();
    if (path.isEmpty()) {
        QMessageBox::warning(this, "打开目录失败", "未设置自动保存路径");
    }
    QFileInfo info{path};
    if (! info.exists()) {
        QDir{path}.mkpath(path);
    }
    if (! info.isDir()) {
        path = QDir::toNativeSeparators(info.path());
        info.setFile(path);
        if (! info.isDir()) {
            return;
        }
    }

#if defined(Q_OS_LINUX)
    if (system(QString("nautilus %1 >/dev/null 2>&1 &").arg(path).toStdString().c_str()) != 0) {
        if (system(QString("thunar %1 >/dev/null 2>&1 &").arg(path).toStdString().c_str()) != 0) {
            if (system(QString("dolphin %1 >/dev/null 2>&1 &").arg(path).toStdString().c_str()) != 0) {
                qDebug() << system(QString("xdg-open %1 >/dev/null 2>&1 &").arg(path).toStdString().c_str());
            }
        }
    }
#else
    QProcess process;
    process.setProgram("cmd.exe");
    process.setArguments({"/C", QString("explorer %1 >NUL 2>&1").arg(path)});
    process.start();
    process.waitForFinished();
    // system(QString("explorer /select,%1 >NUL 2>&1").arg(link).toStdString().c_str());
#endif
}

bool MainWindow::contains(const QPoint &point) {
    if (m_state & State::Rect) {
        return m_rect.contains(point);
    } else if (m_state & State::Free) {
        return m_path.contains(point);
    }
    return false;
}

void MainWindow::updateWindows() {
    m_windows.clear();
    m_index = 0;

#ifdef Q_OS_WINDOWS
    HWND hwnd = GetTopWindow(nullptr);
    QRect rect = getRectByHwnd(hwnd);
    if (rect.isValid()) {
        m_windows.push_back(QRect(rect.left() / m_ratio, rect.top() / m_ratio, rect.width() / m_ratio, rect.height() / m_ratio));
    }

    while ((hwnd = GetNextWindow(hwnd, GW_HWNDNEXT)) != nullptr) {
        QRect rect = getRectByHwnd(hwnd);
        if (rect.isValid()) {
            m_windows.push_back(QRect(rect.left() / m_ratio, rect.top() / m_ratio, rect.width() / m_ratio, rect.height() / m_ratio));
        }
    }
#else
    Display *display = XOpenDisplay(nullptr);
    Window rootWindow = DefaultRootWindow(display);
    Window parent;
    Window* children;
    unsigned int nChildren;
    // 获取所有子窗口
    if (XQueryTree(display, rootWindow, &rootWindow, &parent, &children, &nChildren)) {
        for (int i = nChildren - 1; i >= 0; --i) {
            XWindowAttributes attributes;
            XGetWindowAttributes(display, children[i], &attributes);

            if ((attributes.map_state != IsViewable) || (attributes.width == attributes.height && attributes.width <= 5)) {
                continue;
            }
            m_windows.push_back({static_cast<int>(attributes.x / m_ratio),
                                 static_cast<int>(attributes.y / m_ratio),
                                 static_cast<int>(attributes.width / m_ratio),
                                 static_cast<int>(attributes.height / m_ratio)});
        }

        XFree(children);
    }
    XCloseDisplay(display);
#endif

    const QPoint &point = QCursor::pos();
    for (int i = 0; i < m_windows.size(); ++i) {
        if (m_windows[i].contains(point)) {
            m_index = i;
            break;
        }
    }
}

QImage MainWindow::fullScreenshot() {
    QList<QScreen*> list = QApplication::screens();
    QSize size;
    m_ratio = -1;
    for (auto iter = list.cbegin(); iter != list.cend(); ++iter) {
        QRect rect = (*iter)->geometry();
        qreal ratio = (*iter)->devicePixelRatio();
        if (m_ratio == -1) {
            m_ratio = ratio;
        } else if (m_ratio != ratio) {
            m_ratio = 0;
        }
        size.setWidth(std::max<int>((rect.right() + 1 - rect.left()) * ratio + rect.left(), size.width()));
        size.setHeight(std::max<int>((rect.bottom() + 1 - rect.top()) * ratio + rect.top(), size.height()));
    }
    if (m_ratio <= 0) m_ratio = 1;

    QImage image(size, QImage::Format_ARGB32);
    QPainter painter(&image);
    for (auto iter = list.cbegin(); iter != list.cend(); ++iter) {
        QScreen *screen = (*iter);
        qreal ratio = (*iter)->devicePixelRatio();
        if (ratio == 1) {
            painter.drawPixmap(screen->geometry(), screen->grabWindow(0));
        } else {
            QRect rect = screen->geometry();
            QPixmap pixmap = screen->grabWindow(0);
            qreal ratio = (*iter)->devicePixelRatio();
            painter.drawPixmap(rect.left(), rect.top(), rect.width() * ratio, rect.height() * ratio, pixmap);
        }
    }
    painter.end();
    return image;
}

#ifdef Q_OS_LINUX
QString MainWindow::getWindowTitle(Display *display, Window window) {
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = nullptr;
    Atom _NET_WM_NAME = XInternAtom(display, "_NET_WM_NAME", True);
    Atom UTF8_STRING = XInternAtom(display, "UTF8_STRING", True);
    if (XGetWindowProperty(display, window, _NET_WM_NAME, 0, (~0L), False,
                           UTF8_STRING, &actual_type, &actual_format,
                           &nitems, &bytes_after, &prop) == Success && prop) {
        QString str = QString::fromUtf8(reinterpret_cast<const char*>(prop));
        XFree(prop);
        return str;
    }
    return {};
}
#endif

#ifdef Q_OS_WINDOWS
QRect MainWindow::getRectByHwnd(HWND hwnd) {
    QRect qrect;
    if (IsWindow(hwnd) && IsWindowVisible(hwnd)) {
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
    }
    return qrect;
}

#endif
