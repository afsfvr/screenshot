#include "mainwindow.h"
#include "TopWidget.h"

#include <QShortcut>

MainWindow::MainWindow(QWidget *parent): BaseWindow(parent) {
    m_menu = new QMenu(this);
    m_menu->addAction("截图", this, &MainWindow::start);
    m_menu->addAction("退出", this, &MainWindow::quit);
    m_tray = new QSystemTrayIcon(this);
    m_tray->setContextMenu(m_menu);
    m_tray->setIcon(QIcon(":/images/screenshot.png"));
    m_tray->setToolTip("截图");
    m_tray->show();

    m_state = State::Null;

    m_monitor = new KeyMouseEvent;
    m_monitor->start();
    m_monitor->resume();
    connect(m_monitor, &KeyMouseEvent::keyPress, this, [=](int code, Qt::KeyboardModifiers modifiers){
        if (! this->isVisible() && code == 38 && modifiers == (Qt::ControlModifier | Qt::AltModifier)) {
            start();
        }
    });

    connect(m_tool, &Tool::clickTop, this, [=](){
        if (m_state & State::Free) {
            QRect rect = m_path.boundingRect().toRect();
            QPoint point = rect.topLeft();
            for (auto iter = m_vector.cbegin(); iter != m_vector.cend(); ++iter) {
                (*iter)->translate(- point);
            }
            QImage image = QImage(rect.size(), QImage::Format_ARGB32);
            QPainter painter(&image);
            painter.translate(- point);
            painter.fillPath(m_path, m_image);
            painter.end();
            new TopWidget(image, m_path.translated(- point), m_vector, rect, m_menu);
            this->hide();
        } else if (m_state & State::Rect) {
            QPoint point = m_rect.topLeft();
            for (auto iter = m_vector.cbegin(); iter != m_vector.cend(); ++iter) {
                (*iter)->translate(- point);
            }
            new TopWidget(m_image.copy(m_rect), m_vector, m_rect, m_menu);
            this->hide();
        } else {
            return;
        }
    });
}

MainWindow::~MainWindow() {
}

void MainWindow::mousePressEvent(QMouseEvent *event) {
    if (event->buttons() == event->button()) {
        m_press = true;
        m_point = event->pos();
    }
    if (m_state == State::Null) {
        if (event->button() == Qt::LeftButton) {
            m_state = State::RectScreen;
        } else if (event->button() == Qt::RightButton) {
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
                m_state = State::RectScreen;
            } else if (event->button() == Qt::RightButton) {
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
        if (m_state == State::FreeScreen) {
            m_state = State::FreeEdit;
            m_path.closeSubpath();
            showTool();
            repaint();
        }
    }
    if (event->buttons() == Qt::NoButton) {
        m_press = false;
        unsetCursor();
        if (event->pos() == m_point && event->button() == Qt::RightButton) {
            this->hide();
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
    }
}

void MainWindow::paintEvent(QPaintEvent *event) {
    BaseWindow::paintEvent(event);
    if (m_image.isNull() || m_gray_image.isNull()) return;
    QPainter painter(this);
    painter.setPen(Qt::blue);
    painter.drawImage(rect(), m_gray_image);
    if (m_state & State::Free) {
        painter.fillPath(m_path, m_image);
        painter.drawPath(m_path);
        painter.setClipPath(m_path);
    } else if (m_state & State::Rect) {
        painter.drawImage(m_rect, m_image, m_rect);
        painter.drawRect(m_rect);
    } else {
        return;
    }

    for (auto iter = m_vector.cbegin(); iter != m_vector.cend(); ++iter) {
        (*iter)->draw(painter);
    }
    if (m_shape != nullptr) {
        m_shape->draw(painter);
    }
    if (m_state & State::Screen) {
        painter.setPen(Qt::white);
        painter.setClipping(false);
        QRect rect = (m_state & State::Free) ? m_path.boundingRect().toRect() : m_rect;
        painter.drawText(QCursor::pos() + QPoint(0, 25), QString("(%1,%2 %3x%4)").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height()));
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    m_tool->close();
    BaseWindow::closeEvent(event);
}

void MainWindow::hideEvent(QHideEvent *event) {
    m_tool->hide();
    m_state = State::Null;
    m_path.clear();
    m_image = m_gray_image = QImage();
    clearDraw();
    safeDelete(m_shape);
    resize(1, 1);
    BaseWindow::hideEvent(event);
}

void MainWindow::start() {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    m_image = fullScreenshot();
    m_gray_image = QImage(m_image.size(), QImage::Format_ARGB32);
    for (int x = 0; x < m_image.width(); ++x) {
        for (int y = 0; y < m_image.height(); ++y) {
            QRgb rgb = m_image.pixel(x, y);
            int r = 0.3 * qRed(rgb);
            int g = 0.3 * qGreen(rgb);
            int b = 0.3 * qBlue(rgb);
            // int r = qMax(qRed(rgb) - 100, 0);
            // int g = qMax(qGreen(rgb) - 100, 0);
            // int b = qMax(qBlue(rgb) - 100, 0);
            m_gray_image.setPixel(x, y, qRgb(r, g, b));
        }
    }
    setGeometry(0, 0, m_image.width(), m_image.height());
    setWindowState((windowState() & ~(Qt::WindowMinimized | Qt::WindowMaximized)) | Qt::WindowFullScreen);
    setCursor(Qt::CrossCursor);
    setVisible(true);
    activateWindow();
    m_state = State::Null;
    m_path.clear();
    m_press = false;
    activateWindow();
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

    if (rect.bottom() + m_tool->height() < this->height()) {
        if (rect.right() >= m_tool->width()) {
            point.setX(rect.right() - m_tool->width());
        } else {
            point.setX(rect.right());
        }
        point.setY(rect.bottom());
    } else if (rect.top() >= m_tool->height()) {
        if (rect.right() >= m_tool->width()) {
            point.setX(rect.right() - m_tool->width());
        } else {
            point.setX(rect.right());
        }
        point.setY(rect.top() - m_tool->height());
    }
    m_tool->show();
    m_tool->move(point);
}

void MainWindow::quit() {
    m_tray->hide();
    this->close();
    QApplication::quit();
}

void MainWindow::save(const QString &path) {
    QImage image;
    QPainter painter;
    if (m_state & State::Free) {
        image = QImage(m_path.boundingRect().size().toSize(), QImage::Format_ARGB32);
        painter.begin(&image);
        painter.translate(- m_path.boundingRect().topLeft());
        painter.fillPath(m_path, m_image);
        painter.setClipPath(m_path);
    } else if (m_state & State::Rect) {
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
    } else {
        image.save(path);
    }
    this->hide();
}

bool MainWindow::contains(const QPoint &point) {
    if (m_state & State::Rect) {
        return m_rect.contains(point);
    } else if (m_state & State::Free) {
        return m_path.contains(point);
    }
    return false;
}
