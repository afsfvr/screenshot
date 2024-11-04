#include "BaseWindow.h"

BaseWindow::BaseWindow(QWidget *parent): QWidget{parent}, m_press(false), m_shape(nullptr), m_tool(new Tool(this)) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);

    connect(m_tool, &Tool::save, this, &BaseWindow::save);
    connect(m_tool, &Tool::cancel, this, &BaseWindow::end);
    connect(this, &BaseWindow::choosePath, m_tool, &Tool::choosePath);
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
            save("");
        } else {
            saveColor();
        }
    } else if (event->key() == Qt::Key_S && event->modifiers() == Qt::ControlModifier) {
        emit choosePath();
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
    int width = p2.x() - x;
    int height = p2.y() - y;
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

