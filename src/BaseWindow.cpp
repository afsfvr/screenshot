#include "BaseWindow.h"

BaseWindow::BaseWindow(QWidget *parent): QWidget{parent}, m_press{false}, m_shape{nullptr}, m_tool{new Tool{this}}, m_edit{nullptr}, m_ignore{false} {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);

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

    m_cursor = Qt::ArrowCursor;
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
            Text *text = dynamic_cast<Text*>(m_vector.last());
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
        update();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void BaseWindow::timerEvent(QTimerEvent *event) {
    BaseWindow::removeTip(event->timerId());
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
    Text *text = dynamic_cast<Text*>(m_shape);
    if (text) {
        m_edit->setFixedWidth(150);
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

        m_edit->move(mapToParent(QPoint(point.x() - 6, point.y() - qRound((m_edit->height() - m_edit->fontMetrics().height()) / 2.0))));
        int max = getGeometry().right() - m_edit->x();
        if (m_edit->width() > max) {
            m_edit->setMinimumWidth(max);
        }
        m_edit->setMaximumWidth(max);
    }
}

QPoint BaseWindow::getScreenPoint(const QPoint &point) {
    if (m_ratio == 1) return point;

    QPoint pos = point * m_ratio;
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

    return {x, y};
}

QRect BaseWindow::getScreenRect(const QRect &rect) {
    if (m_ratio == 1) return rect;

    return {getScreenPoint(rect.topLeft()), rect.size()};
}

void BaseWindow::undo() {
    if (! m_vector.empty() && ! m_press) {
        m_stack.push(m_vector.takeLast());
        update();
    }
}

void BaseWindow::redo() {
    if (! m_stack.empty() && ! m_press) {
        m_vector.push_back(m_stack.pop());
        update();
    }
}

void BaseWindow::saveColor() {
    if (! m_image.isNull()) {
        QClipboard *clipboard = QApplication::clipboard();
        if (clipboard) {
            clipboard->setText(m_image.pixelColor(m_mouse_pos * m_ratio).name().toUpper());
            addTip("复制颜色成功");
        } else {
            addTip("复制颜色失败");
        }
    }
}

int BaseWindow::addTip(const QString &text, int duration) {
    if (duration <= 0) {
        return -1;
    }

    int id = startTimer(duration);
    if (id < 0) {
        return -1;
    }

    qint64 time = QDateTime::currentMSecsSinceEpoch();
    m_tips.insert(0, { id, text, time, duration });
    update();
    return id;
}

bool BaseWindow::removeTip(int id) {
    if (id < 0) {
        return false;
    }
    for (auto iter = m_tips.begin(); iter != m_tips.end(); ++iter) {
        if (id == iter->id) {
            killTimer(id);
            m_tips.erase(iter);
            update();
            return true;
        }
    }
    return false;
}

bool BaseWindow::removeTip(const QString &text) {
    bool ret = false;
    for (auto iter = m_tips.begin(); iter != m_tips.end(); ++iter) {
        if (text == iter->text) {
            killTimer(iter->id);
            iter = m_tips.erase(iter);
            ret = true;
        }
    }
    if (ret) {
        update();
    }
    return ret;
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

void BaseWindow::drawTips(QPainter &painter) {
    QPen backupPen = painter.pen();
    QFont backupFont = painter.font();
    QFont font = backupFont;
    font.setPixelSize(16);
    painter.setPen(Qt::red);
    painter.setFont(font);

    QFontMetrics fm{painter.font()};
    int centerX = this->width() / 2;
    int maxTextWidth = this->width() * 0.9;
    int y = this->height() - fm.height() / 2 - 10;
    qint64 time = QDateTime::currentMSecsSinceEpoch();

    for (auto iter = m_tips.begin(); iter < m_tips.end();) {
        if (time < iter->time + iter->duration) {
            QString text = iter->text;

            QRect textRect = fm.boundingRect(0, 0, maxTextWidth, 1000, Qt::TextWordWrap, text);
            int x = centerX - textRect.width() / 2;
            QRect drawRect(x, y - textRect.height() + fm.ascent(), textRect.width(), textRect.height());

            painter.fillRect(drawRect, QColor(0, 0, 0, 150));
            painter.drawText(drawRect, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, text);

            y -= textRect.height() + 10;
            ++iter;
        } else {
            killTimer(iter->id);
            iter = m_tips.erase(iter);
        }
    }

    painter.setPen(backupPen);
    painter.setFont(backupFont);
}

void BaseWindow::setCursorShape(Qt::CursorShape cursor) {
    if (m_cursor != cursor) {
        if (cursor == Qt::BitmapCursor) {
            setCursor(QCursor(QPixmap(":/images/pencil.png"), 0, 24));
        } else {
            setCursor(cursor);
        }
        m_cursor = cursor;
    }
}

