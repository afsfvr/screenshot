#include "mainwindow.h"
#include "TopWidget.h"
#include "GifWidget.h"

#include <QShortcut>
#include <QMessageBox>
#include <QTimer>
#include <QStandardPaths>

MainWindow::MainWindow(QWidget *parent): BaseWindow(parent) {
    m_hotkey = new HotKeyWidget{&m_capture, &m_record};
    connect(m_hotkey, &HotKeyWidget::capture, this, &MainWindow::updateCapture);
    connect(m_hotkey, &HotKeyWidget::record, this, &MainWindow::updateRecord);

    m_menu = new QMenu(this);
    m_menu->addAction("修改快捷键", this, &MainWindow::updateHotkey);
    m_action1 = m_menu->addAction("截图", this, &MainWindow::start);
    m_action2 = m_menu->addAction("录制GIF", this, [=](){
        m_gif = true;
        start();
    });
    m_menu->addAction("退出", this, &MainWindow::quit);
    m_tray = new QSystemTrayIcon(this);
    m_tray->setContextMenu(m_menu);
    m_tray->setIcon(QIcon(":/images/screenshot.ico"));
    m_tray->setToolTip("截图");
    m_tray->show();

    m_state = State::Null;
    m_resize = ResizeImage::NoResize;
    m_gif = false;

    initHotKey();
    setMouseTracking(true);

    connect(m_tool, &Tool::clickTop, this, &MainWindow::top);
}

MainWindow::~MainWindow() {
    saveHotKey();
    safeDelete(m_hotkey);

#ifdef Q_OS_LINUX
    delete m_monitor;
#endif
#ifdef Q_OS_WINDOWS
    UnregisterHotKey((HWND)this->winId(), 1);
    UnregisterHotKey((HWND)this->winId(), 2);
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
            m_state = State::RectScreen;
        } else if (! m_gif && event->button() == Qt::RightButton) {
            m_state = State::FreeScreen;
            m_path.moveTo(event->pos());
        }
        m_tool->hide();
    } else if (m_state & State::Edit) {
        if (m_resize != ResizeImage::NoResize) {
            clearDraw();
            m_tool->hide();
        } else if (contains(event->pos())) {
            if (event->button() == Qt::LeftButton) {
                if (m_tool->isDraw()) {
                    setShape(event->pos());
                } else {
                    m_tool->hide();
                }
            }
        } else {
            if (event->button() == Qt::LeftButton) {
                clearDraw();
                m_state = State::RectScreen;
            } else if (! m_gif && event->button() == Qt::RightButton) {
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
            repaint();
        } else if (m_state & State::Edit) {
            if (cursor().shape() == Qt::BitmapCursor) {
                if (! contains(event->pos())) {
                    unsetCursor();
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
            repaint();
            showTool();
        }
    } else if (event->button() == Qt::RightButton) {
        if (m_state == State::FreeScreen && m_path.elementCount() > 1) {
            m_state = State::FreeEdit;
            m_path.closeSubpath();
            showTool();
            repaint();
        }
    }
    if (event->buttons() == Qt::NoButton) {
        m_press = false;
        if ((event->x() == m_point.x() || event->y() == m_point.y()) && event->button() == Qt::RightButton) {
            if (m_state != State::Null && isValid()) {
                setCursor(Qt::CrossCursor);
                clearDraw();
                m_state = State::Null;
                m_tool->hide();
                repaint();
            } else {
                unsetCursor();
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
            repaint();
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
        } if (shape == Qt::SizeAllCursor) {
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
            repaint();
        } else if (shape == Qt::BitmapCursor) {
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
            repaint();
        }
    } else if (event->buttons() == Qt::NoButton) {
        if (m_state == State::Null) {
            setCursor(Qt::CrossCursor);
            for (int i = 0; i < m_windows.size(); ++i) {
                if (m_windows[i].contains(event->pos())) {
                    if (i != m_index) {
                        m_index = i;
                    }
                    break;
                }
            }
            repaint();
        } else if (m_state == State::RectEdit) {
            m_resize = ResizeImage::NoResize;
            QPoint point = event->pos();
            if (m_rect.contains(point)) {
                if (m_tool->isDraw()) {
                    setCursor(QCursor(QPixmap(":/images/pencil.png"), 0, 24));
                } else {
                    setCursor(Qt::SizeAllCursor);
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
                    unsetCursor();
                } else {
                    if (m_resize == ResizeImage::TopLeft || m_resize == ResizeImage::BottomRight) {
                        setCursor(Qt::SizeFDiagCursor);
                    } else if (m_resize == ResizeImage::TopRight || m_resize == ResizeImage::BottomLeft) {
                        setCursor(Qt::SizeBDiagCursor);
                    } else if (m_resize == ResizeImage::Top || m_resize == ResizeImage::Bottom) {
                        setCursor(Qt::SizeVerCursor);
                    } else if (m_resize == ResizeImage::Left || m_resize == ResizeImage::Right) {
                        setCursor(Qt::SizeHorCursor);
                    } else {
                        unsetCursor();
                    }
                }
            }
        } else if (m_state == State::FreeEdit) {
            if (m_path.contains(event->pos())) {
                if (m_tool->isDraw()) {
                    setCursor(QCursor(QPixmap(":/images/pencil.png"), 0, 24));
                } else {
                    setCursor(Qt::SizeAllCursor);
                }
            } else {
                unsetCursor();
            }
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
        painter.setClipRect(m_rect);
    } else {
        if (m_index >= 0 && m_index < m_windows.size()) {
            rect = m_windows[m_index];
            painter.drawImage(rect, m_image, rect);
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

    if (m_state == State::Null || (m_state & State::Screen)) {
        const QPoint &cursor = QCursor::pos();
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
        painter.drawImage(QRect(point.x(), point.y(), 84, 84), m_image, QRect(cursor.x() - 10, cursor.y() - 10, 21, 21));
        painter.drawLine(point.x(), point.y() + 42, point.x() + 84, point.y() + 42);
        painter.drawLine(point.x() + 42, point.y(), point.x() + 42, point.y() + 84);
        QColor color = m_image.pixelColor(cursor);
        QFont font = painter.font();
        font.setPixelSize(13);
        painter.setFont(font);
        painter.drawText(point.x(), point.y() + 97, QString("RGB: %1").arg(color.name().toUpper()));
        painter.drawText(point.x(), point.y() + 111, QString("按C复制颜色"));
        painter.drawText(point.x(), point.y() + 125, QString("%1, %2").arg(cursor.x()).arg(cursor.y()));
        painter.drawText(point.x(), point.y() + 138, QString("(%1 x %2)").arg(rect.width()).arg(rect.height()));
    }

    drawTips(painter);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    m_tool->close();
    BaseWindow::closeEvent(event);
}

#ifdef Q_OS_WINDOWS
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, long *result) {
    if (! this->isVisible() && eventType == "windows_generic_MSG") {
        MSG *msg = static_cast<MSG *>(message);
        if (msg->message == WM_HOTKEY) {
            if (msg->wParam == 1) {
                start();
                return true;
            } else if (msg->wParam == 2) {
                m_gif = true;
                start();
                return true;
            }
        }
    }
    return QWidget::nativeEvent(eventType, message, result);
}
#endif

void MainWindow::start() {
    updateWindows();
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
    setGeometry(0, 0, m_image.width(), m_image.height());
    setWindowState((windowState() & ~(Qt::WindowMinimized | Qt::WindowMaximized)) | Qt::WindowFullScreen);
    setCursor(Qt::CrossCursor);
    setVisible(true);
    m_state = State::Null;
    m_resize = ResizeImage::NoResize;
    m_path.clear();
    m_press = false;
    activateWindow();
    repaint();
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
    m_tool->move(point);
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

void MainWindow::updateHotkey() {
    m_hotkey->setConfigPath(getConfigPath());
    if (m_hotkey->isMinimized()) {
        m_hotkey->showNormal();
    } else if (m_hotkey->isVisible()) {
        m_hotkey->activateWindow();
        m_hotkey->raise();
    } else {
        m_hotkey->show();
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
        if (m_capture.modifiers != Qt::NoModifier && m_capture.modifiers == modifiers && m_capture.key == static_cast<quint32>(code)) {
            start();
        } else if (m_record.modifiers != Qt::NoModifier && m_record.modifiers == modifiers && m_record.key == static_cast<quint32>(code)) {
            m_gif = true;
            start();
        }
    }
}
#endif

void MainWindow::updateCapture() {
#ifdef Q_OS_WINDOWS
    UnregisterHotKey((HWND)this->winId(), 1);
    quint32 fsModifiers = 0;
#endif
    if (m_capture == m_record && m_capture.modifiers != Qt::NoModifier) {
        QMessageBox::warning(this, "注册热键失败", "不能重复");
        m_capture.modifiers = Qt::NoModifier;
    }
    if (m_capture.modifiers != Qt::NoModifier) {
        QString s{"截图("};
        if (m_capture.modifiers & Qt::ControlModifier) {
#ifdef Q_OS_WINDOWS
            fsModifiers |= MOD_CONTROL;
#endif
            s.append("Ctrl+");
        }
        if (m_capture.modifiers & Qt::AltModifier) {
#ifdef Q_OS_WINDOWS
            fsModifiers |= MOD_ALT;
#endif
            s.append("Alt+");
        }
        if (m_capture.modifiers & Qt::ShiftModifier) {
#ifdef Q_OS_WINDOWS
            fsModifiers |= MOD_SHIFT;
#endif
            s.append("Shift+");
        }
#if defined(Q_OS_WINDOWS)
        if (RegisterHotKey((HWND)this->winId(), 1, fsModifiers, m_capture.key)) {
            s.append(static_cast<char>(m_capture.key)).append(")");
            m_action1->setText(s);
        } else {
            QMessageBox::warning(this, "注册热键失败", "截图注册热键失败");
            m_capture.modifiers = Qt::NoModifier;
            m_action1->setText("截图");
        }
#else
        s.append(static_cast<char>(m_capture.key)).append(")");
        m_action1->setText(s);
#endif
    } else {
        m_action1->setText("截图");
    }
}

void MainWindow::updateRecord() {
#ifdef Q_OS_WINDOWS
    UnregisterHotKey((HWND)this->winId(), 2);
    quint32 fsModifiers = 0;
#endif
    if (m_capture == m_record && m_capture.modifiers != Qt::NoModifier) {
        QMessageBox::warning(this, "注册热键失败", "不能重复");
        m_record.modifiers = Qt::NoModifier;
    }
    if (m_record.modifiers != Qt::NoModifier) {
        QString s{"录制GIF("};
        if (m_record.modifiers & Qt::ControlModifier) {
#ifdef Q_OS_WINDOWS
            fsModifiers |= MOD_CONTROL;
#endif
            s.append("Ctrl+");
        }
        if (m_record.modifiers & Qt::AltModifier) {
#ifdef Q_OS_WINDOWS
            fsModifiers |= MOD_ALT;
#endif
            s.append("Alt+");
        }
        if (m_record.modifiers & Qt::ShiftModifier) {
#ifdef Q_OS_WINDOWS
            fsModifiers |= MOD_SHIFT;
#endif
            s.append("Shift+");
        }
#if defined(Q_OS_WINDOWS)
        if (RegisterHotKey((HWND)this->winId(), 2, fsModifiers, m_record.key)) {
            s.append(static_cast<char>(m_record.key)).append(")");
            m_action2->setText(s);
        } else {
            QMessageBox::warning(this, "注册热键失败", "GIF注册热键失败");
            m_record.modifiers = Qt::NoModifier;
            m_action2->setText("录制GIF");
        }
#else
        s.append(static_cast<char>(m_record.key)).append(")");
        m_action2->setText(s);
#endif
    } else {
        m_action2->setText("录制GIF");
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
    m_state = State::Null;
    m_resize = ResizeImage::NoResize;
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

QString MainWindow::getConfigPath() {
    QString path = QDir::toNativeSeparators(QApplication::applicationDirPath() + QDir::separator() + QApplication::applicationName() + ".data");
    if (QFile::exists(path)) {
        return path;
    } else {
        path = QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
        if (! QFile::exists(path)) {
            qDebug() << QString("创建文件夹%1: %2").arg(path, QDir{}.mkdir(path) ? "成功" : "失败");
        }
        return path + QDir::separator() + QApplication::applicationName() + ".data";;
    }
}

void MainWindow::initHotKey() {
    m_capture.modifiers = Qt::ControlModifier | Qt::AltModifier;
    m_capture.key = 'A';
    m_record.modifiers = Qt::NoModifier;
    m_record.key = 'A';

    QFile file{getConfigPath()};
    if (file.exists() && file.open(QFile::ReadOnly) && file.size() == 16) {
        QByteArray array = file.readAll();
        const char *data = array.constData();
        m_capture.modifiers = static_cast<Qt::KeyboardModifiers>(*reinterpret_cast<const quint32*>(data));
        m_capture.key = *reinterpret_cast<const quint32*>(data + 4);
        if ((m_capture.modifiers & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier)) == Qt::NoModifier || m_capture.key < 'A' || m_capture.key > 'Z') {
            m_capture.modifiers = Qt::NoModifier;
            m_capture.key = 'A';
        }

        m_record.modifiers = static_cast<Qt::KeyboardModifiers>(*reinterpret_cast<const quint32*>(data + 8));
        m_record.key = *reinterpret_cast<const quint32*>(data + 12);
        if ((m_record.modifiers & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier)) == Qt::NoModifier || m_record.key < 'A' || m_record.key > 'Z') {
            m_record.modifiers = Qt::NoModifier;
            m_record.key = 'A';
        }
    } else {
        saveHotKey();
    }

#ifdef Q_OS_LINUX
    m_monitor = new KeyMouseEvent;
    m_monitor->start();
    m_monitor->resume();
    connect(m_monitor, &KeyMouseEvent::keyPress, this, &MainWindow::keyPress);
#endif

    updateCapture();
    updateRecord();
}

void MainWindow::saveHotKey() {
    QFile file{getConfigPath()};
    if (file.open(QFile::WriteOnly | QFile::Truncate)) {
        quint32 arr[4];
        arr[0] = m_capture.modifiers;
        arr[1] = m_capture.key;
        arr[2] = m_record.modifiers;
        arr[3] = m_record.key;
        file.write(reinterpret_cast<const char*>(arr), 16);
        file.close();
    } else {
        qWarning() << "write config failed: " << file.errorString();
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

void MainWindow::updateWindows() {
    m_windows.clear();
    m_index = 0;

#ifdef Q_OS_WINDOWS
    HWND hwnd = GetTopWindow(nullptr);
    addRect(hwnd);

    while ((hwnd = GetNextWindow(hwnd, GW_HWNDNEXT)) != nullptr) {
        addRect(hwnd);
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
            m_windows.push_back({attributes.x, attributes.y, attributes.width, attributes.height});
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

#ifdef Q_OS_WINDOWS
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
