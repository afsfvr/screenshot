#include "BaseWindow.h"

BaseWindow::BaseWindow(QWidget *parent): QWidget{parent}, m_press{false}, m_shape{nullptr}, m_tool{new Tool{this}}, m_edit{nullptr}, m_ignore{false} {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_X11NetWmWindowTypeDesktop);

    connect(m_tool, &Tool::undo, this, &BaseWindow::undo);
    connect(m_tool, &Tool::redo, this, &BaseWindow::redo);
    connect(m_tool, &Tool::save, this, &BaseWindow::save);
    connect(m_tool, &Tool::cancel, this, &BaseWindow::end);
    connect(this, &BaseWindow::choosePath, m_tool, &Tool::choosePath, static_cast<Qt::ConnectionType>(Qt::AutoConnection | Qt::UniqueConnection));

    m_edit = new QLineEdit{this};
    m_edit->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    m_edit->setAttribute(Qt::WA_TranslucentBackground);
    m_edit->setContextMenuPolicy(Qt::NoContextMenu);
    m_edit->installEventFilter(this);
    connect(m_edit, &QLineEdit::textEdited, this, [=](const QString &text) {
        int width = m_edit->fontMetrics().horizontalAdvance(text) + 20;
        m_edit->setMinimumWidth(std::min(width, m_edit->maximumWidth()));
    });
}

BaseWindow::~BaseWindow() {
    clearDraw();
    m_path.clear();
    delete m_tool;
}

void BaseWindow::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        if (! m_press) {
            end();
        }
    } else if (event->key() == Qt::Key_C) {
        if (event->modifiers() == Qt::ControlModifier) {
            if (isValid()) {
                save("");
            }
        } else {
            saveColor();
        }
    } else if (event->key() == Qt::Key_S && event->modifiers() == Qt::ControlModifier) {
        if (isValid()) {
            emit choosePath();
        }
    } else if (event->key() == Qt::Key_Z && event->modifiers() == Qt::ControlModifier) {
        undo();
    } else if (event->key() == Qt::Key_Y && event->modifiers() == Qt::ControlModifier) {
        redo();
    }
}

void BaseWindow::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->buttons() == Qt::LeftButton && ! m_press) {
        save("");
    }
}

bool BaseWindow::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_edit && event->type() == QEvent::FocusOut) {
        m_ignore = (QApplication::focusObject() == this);
        if (! m_vector.isEmpty()) {
            Text *text = qobject_cast<Text*>(m_vector.last());
            if (text) {
                const QString &s = m_edit->text();
                if (s.isEmpty()) {
                    delete m_vector.takeLast();
                } else {
                    text->setText(m_edit->text());
                }
            }
        }
        m_edit->clear();
        m_edit->hide();
        repaint();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

QImage BaseWindow::fullScreenshot() {
    QList<QScreen*> list = QApplication::screens();
    QSize size;
    for (auto iter = list.cbegin(); iter != list.cend(); ++iter) {
        QRect rect = (*iter)->geometry();
        size.setWidth(std::max(rect.right() + 1, size.width()));
        size.setHeight(std::max(rect.bottom() + 1, size.height()));
    }

    QImage image(size, QImage::Format_ARGB32);
    QPainter painter(&image);
    for (auto iter = list.cbegin(); iter != list.cend(); ++iter) {
        QScreen *screen = (*iter);
        painter.drawImage(screen->geometry(), screen->grabWindow(0).toImage());
    }
    painter.end();
    return image;
}

QRect BaseWindow::getRect(const QPoint &p1, const QPoint &p2) {
    int x = p1.x();
    int y = p1.y();
    int width = p2.x() - x + 1;
    int height = p2.y() - y + 1;
    if (width < 0) {
        x = p2.x();
        width = -width;
    }
    if (height < 0) {
        y = p2.y();
        height = -height;
    }
    return QRect(x, y, width, height);
}

void BaseWindow::setShape(const QPoint &point) {
    if (m_ignore) {
        m_ignore = false;
        return;
    }
    if (m_shape != nullptr) {
        qWarning() << "error" << m_shape;
        safeDelete(m_shape);
    }
    m_shape = m_tool->getShape(point);
    Text *text = qobject_cast<Text*>(m_shape);
    if (text) {
        m_edit->setMinimumWidth(150);
        m_edit->setMaximumWidth(150);
        m_edit->setFont(text->font());
        m_edit->setWindowOpacity(m_shape->opacity());
        const QPen &pen = m_shape->pen();
        m_edit->setStyleSheet(QString("QLineEdit {"
                                      " background: transparent;"
                                      " border: 2px solid red;"
                                      " color: %1;"
                                      " font-size: %2px;"
                                      " padding: 2px; }").arg(pen.color().name()).arg(pen.width()));
        m_edit->show();
        m_edit->activateWindow();
        m_edit->setFocus();

        m_edit->move(mapToParent({point.x() - 6, point.y() - qRound((m_edit->height() - m_edit->fontMetrics().height()) / 2.0)}));
        int max = getGeometry().right() - m_edit->x();
        if (m_edit->width() > max) {
            m_edit->setMinimumWidth(max);
        }
        m_edit->setMaximumWidth(max);
    }
}

void BaseWindow::undo() {
    if (! m_vector.empty() && ! m_press) {
        m_stack.push(m_vector.takeLast());
        repaint();
    }
}

void BaseWindow::redo() {
    if (! m_stack.empty() && ! m_press) {
        m_vector.push_back(m_stack.pop());
        repaint();
    }
}

void BaseWindow::saveColor() {
    if (! m_image.isNull()) {
        QPoint point = QCursor::pos();
        QClipboard *clipboard = QApplication::clipboard();
        if (clipboard) {
            clipboard->setText(m_image.pixelColor(point).name().toUpper());
        }
    }
}

void BaseWindow::clearDraw()
{
    for (auto iter = m_vector.begin(); iter != m_vector.end(); ++iter) {
        delete (*iter);
    }
    m_vector.clear();
    clearStack();
    safeDelete(m_shape);
}

void BaseWindow::clearStack()
{
    for (auto iter = m_stack.begin(); iter != m_stack.end(); ++iter) {
        delete (*iter);
    }
    m_stack.clear();
}

