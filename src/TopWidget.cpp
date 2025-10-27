#include "TopWidget.h"
#include "mainwindow.h"

#include <QTimer>
#include <QtMath>
#include <QHBoxLayout>
#include <QPushButton>

#ifdef Q_OS_LINUX
#include <X11/Xlib.h>
#include <QX11Info>
#undef KeyPress
#endif

TopWidget::TopWidget(QImage &image, const QRect &rect, QMenu *menu, qreal ratio): m_max_offset{image.height() - qRound(rect.height() * ratio)}, m_origin{rect.size()} {
    m_ratio = ratio;
    m_image = std::move(image);
    tray_menu = menu;
    QSize size = rect.size();
    setFixedSize(size);
    setGeometry(getScreenRect(rect));
#ifdef OCR
    m_center = QPoint(size.width() / 2, size.height() / 2);
    m_radius = qMin(size.width(), size.height()) / 4;
    m_radius1 = qRound(m_radius / 8.0);
#endif
    init();
}

TopWidget::TopWidget(QImage &&image, QVector<Shape *> &vector, const QRect &rect, QMenu *menu, qreal ratio): m_origin{rect.size()} {
    m_ratio = ratio;
    m_image = std::move(image);
    m_vector = std::move(vector);
    tray_menu = menu;
    QSize size = rect.size();
    setFixedSize(size);
    setGeometry(getScreenRect(rect));
#ifdef OCR
    m_center = QPoint(size.width() / 2, size.height() / 2);
    m_radius = qMin(size.width(), size.height()) / 4;
    m_radius1 = qRound(m_radius / 8.0);
#endif
    init();
}

TopWidget::TopWidget(QImage &image, QPainterPath &&path, QVector<Shape *> &vector, const QRect &rect, QMenu *menu, qreal ratio): m_origin{rect.size()} {
    m_ratio = ratio;
    m_image = std::move(image);
    m_path = std::move(path);
    m_vector = std::move(vector);
    tray_menu = menu;
    QSize size = rect.size();
    setFixedSize(size);
    setGeometry(getScreenRect(rect));
#ifdef OCR
    m_center = QPoint(size.width() / 2, size.height() / 2);
    m_radius = qMin(size.width(), size.height()) / 4;
    m_radius1 = qRound(m_radius / 8.0);
#endif
    init();
}

TopWidget::~TopWidget() {
    delete m_menu;
    m_menu = nullptr;
    if (m_scroll_timer != -1) {
        killTimer(m_scroll_timer);
        m_scroll_timer = -1;
    }
    if (m_ratio_timer != -1) {
        killTimer(m_ratio_timer);
        m_ratio_timer = -1;
    }
#ifdef OCR
    if (m_ocr_timer != -1) {
        OcrInstance->cancel(this);
        killTimer(m_ocr_timer);
        m_ocr_timer = -1;
    }
    if (m_widget != nullptr) {
        delete m_widget;
        m_widget = nullptr;
        m_text = nullptr;
        m_button = nullptr;
    }
#endif
}

void TopWidget::showTool() {
    if (m_disable_tool && m_disable_tool->isChecked()) return;
    QPoint point;

    QRect rect = geometry();

    if (rect.bottom() + m_tool->height() < m_screen_size.height()) {
        if (rect.right() > m_screen_size.width()) { // 左下
            if (rect.left() + m_tool->width() > m_screen_size.width()) {
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
        if (rect.right() > m_screen_size.width()) { // 左上
            if (rect.left() + m_tool->width() > m_screen_size.width()) {
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

void TopWidget::scaleKeyChanged(bool value) {
    m_scale_ctrl = value;
}

#ifdef OCR
void TopWidget::ocrStart() {
    if (m_ocr.isEmpty()) {
        if (m_ocr_timer != -1) {
            return;
        }
        m_angle = 0;
        m_ocr_timer = startTimer(50);
        OcrInstance->ocr(this, m_image);
    } else {
        m_ocr.clear();
        if (m_ocr_timer != -1) {
            killTimer(m_ocr_timer);
            m_ocr_timer = -1;
        }
        update();
    }
}

void TopWidget::ocrEnd(const QVector<Ocr::OcrResult> &result) {
    if (result.size() == 1 && result[0].score == -1) {
        m_ocr.clear();
        addTip(result[0].text, 3000);
    } else {
        if (result.size() == 0) {
            addTip("无结果");
        }
        m_ocr = result;
        if (m_ratio != 1) {
            for (auto iter = result.begin(); iter != result.end(); ++iter) {
                QTransform transform;
                transform.scale(1 / m_ratio, 1 / m_ratio);
                m_ocr.push_back({transform.map(iter->path), iter->text, iter->score});
            }
        } else {
            m_ocr = result;
        }
    }

    if (m_ocr_timer != -1) {
        killTimer(m_ocr_timer);
        m_ocr_timer = -1;
        update();
    }
}
#endif

#ifdef Q_OS_LINUX
void TopWidget::mouseRelease(QSharedPointer<QMouseEvent> event) {
    if (m_move && event->button() == Qt::LeftButton) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        QPoint gpos = event->globalPosition().toPoint();
#else
        QPoint gpos = event->globalPos();
#endif
        QPoint screenPos = gpos;
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
        QMouseEvent *e = new QMouseEvent(QEvent::MouseButtonRelease, gpos - geometry().topLeft(), gpos, screenPos, Qt::LeftButton, event->buttons(), event->modifiers(), event->source());
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
                hideWidget(false);
            }
        } else if (event->type() == QEvent::Wheel) {
            wheelEvent(dynamic_cast<QWheelEvent*>(event));
        }
    }
    return BaseWindow::eventFilter(watched, event);
}
#endif

void TopWidget::timerEvent(QTimerEvent *event) {
    if (event->timerId() == m_scroll_timer) {
        killTimer(m_scroll_timer);
        m_scroll_timer = -1;
        update();
    } else if (event->timerId() == m_ratio_timer) {
        killTimer(m_ratio_timer);
        m_ratio_timer = -1;
        update();
#ifdef OCR
    } else if (event->timerId() == m_ocr_timer) {
        m_angle = (m_angle + 30) % 360;
        update();
#endif
    } else {
        BaseWindow::timerEvent(event);
    }
}

void TopWidget::keyPressEvent(QKeyEvent *event) {
    BaseWindow::keyPressEvent(event);
    if (m_lock_pos && m_lock_pos->isChecked()) return;
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
    qInfo().noquote() << QString("关闭置顶窗口:(%1,%2 %3x%4)").arg(x()).arg(y()).arg(m_image.width()).arg(m_image.height());
    m_tool->close();
    QWidget::closeEvent(event);
    this->deleteLater();
}

void TopWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.scale(width()  / static_cast<float>(m_origin.width()), height() / static_cast<float>(m_origin.height()));
    if (m_path.elementCount() < 2) {
        QRect target = QRect(0, 0, m_image.width() / m_ratio, m_image.height() / m_ratio);
        QRect source = QRect(0, m_offsetY, m_image.width(), m_image.height());
        painter.drawImage(target, m_image, source);
    } else {
        QBrush brush{m_image};
        if (m_ratio != 1) {
            QTransform transform;
            transform.scale(1.0 / m_ratio, 1.0 / m_ratio);
            brush.setTransform(transform);
        }
        painter.fillPath(m_path, brush);
        painter.setClipPath(m_path);
    }

    if (m_max_offset > 0) {
        painter.save();
        painter.translate(0, - m_offsetY);
    }
    for (auto iter = m_vector.cbegin(); iter != m_vector.cend(); ++iter) {
        (*iter)->draw(painter);
    }
    if (m_shape != nullptr) {
        m_shape->draw(painter);
    }

#ifdef OCR
    painter.setPen(Qt::red);
    for (auto iter = m_ocr.cbegin(); iter != m_ocr.cend(); ++iter) {
        painter.drawPath(iter->path);
    }
    if (m_max_offset > 0) {
        painter.restore();
    }
    if (m_ocr_timer != -1) {
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(QRect{QPoint{0, 0}, m_origin}, QColor(255, 255, 255, 150));
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
#else
    if (m_max_offset > 0) {
        painter.restore();
    }
#endif
    if (m_scroll_timer != -1) {
        int maxH = m_image.height() / m_ratio;
        int h = m_origin.height();

        int sliderHeight = qMax<int>(static_cast<float>(h) / maxH * h, 20); // 最小高度为20
        float offsetRatio = static_cast<float>(m_offsetY) / m_max_offset;
        int sliderY = static_cast<int>((h - sliderHeight) * offsetRatio);

        QRect sliderRect(m_origin.width() - 10, sliderY, 10, sliderHeight);

        painter.fillRect(sliderRect, QColor(100, 100, 100));
    }
    if (m_ratio_timer != -1) {
        painter.setPen(Qt::red);
        float ratio = static_cast<float>(width()) / m_origin.width();
        QString text = QString("%1%").arg(qRound(ratio * 100));
        if (ratio > 1) ratio = 1.f;

        const QFont font = painter.font();
        QFont f = font;
        f.setPointSize(f.pointSize() / ratio);
        painter.setFont(f);
        painter.drawText(10 / ratio, 20 / ratio, text);
        painter.setFont(font);
    }

    drawTips(painter);
}

void TopWidget::hideEvent(QHideEvent *event) {
    m_tool->hide();
    BaseWindow::hideEvent(event);
}

void TopWidget::mousePressEvent(QMouseEvent *event) {
    m_mouse_pos = event->pos();
    if (event->buttons() == Qt::LeftButton) {
        m_press = true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        m_point = event->globalPosition().toPoint() - geometry().topLeft();
#else
        m_point = event->globalPos() - geometry().topLeft();
#endif
        QPoint point = event->pos() / m_scale_ratio;
        point.ry() += m_offsetY;
        if (m_tool->isDraw()) {
            setCursorShape(Qt::BitmapCursor);
#ifdef OCR
            if (m_ocr_timer == -1) {
                setShape(point);
            }
#else
            setShape(point);
#endif
        } else {
            setCursorShape(Qt::SizeAllCursor);
#ifdef OCR
            m_widget->hide();
#endif
            m_tool->hide();
            if (m_move_shape) {
                m_move_shape->moveEnd();
                m_move_shape = nullptr;
            }
            if (m_disable_edit == nullptr || ! m_disable_edit->isChecked()) {
                for (auto iter = m_vector.begin(); iter != m_vector.end(); ++iter) {
                    if((*iter)->canMove(point)) {
                        m_move_shape = *iter;
                        break;
                    }
                }
            }
        }
    } else if (event->buttons() == Qt::RightButton) {
        if (m_right_menu) {
            m_right_menu->show();
            QPoint gpos = event->globalPos();
            int right = gpos.x() + m_right_menu->width();
            int bottom = gpos.y() + m_right_menu->height();
            if (bottom > m_screen_size.height() || right > m_screen_size.width()) {
                gpos.rx() -= m_right_menu->width();
                gpos.ry() -= m_right_menu->height();
            }
            m_right_menu->move(gpos);
            m_tool->hide();
        }
    }
}

void TopWidget::mouseReleaseEvent(QMouseEvent *event) {
    m_mouse_pos = event->pos();
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
            update();
        }
#ifdef Q_OS_LINUX
        m_move = false;
#endif
        m_press = false;
        if (m_move_shape) {
            m_move_shape->moveEnd();
            m_move_shape = nullptr;
        }
        showTool();
    }
}

void TopWidget::mouseMoveEvent(QMouseEvent *event) {
    m_mouse_pos = event->pos();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QPoint gpos = event->globalPosition().toPoint();
#else
    QPoint gpos = event->globalPos();
#endif
    if (event->buttons() == Qt::NoButton) {
        if (m_tool->isDraw()) {
            setCursorShape(Qt::BitmapCursor);
        } else {
            setCursorShape(Qt::SizeAllCursor);
        }
#ifdef OCR
        if (m_widget->isHidden() || ! m_widget->geometry().contains(gpos)) {
            bool hide = true;
            QPoint point = event->pos() / m_scale_ratio;
            if (m_max_offset > 0) {
                point.ry() += m_offsetY;
            }
            for (auto iter = m_ocr.cbegin(); iter != m_ocr.cend(); ++iter) {
                if (iter->path.contains(point)) {
                    QString ptr = QString::number(reinterpret_cast<quintptr>(&(iter->text)));
                    if (ptr != m_text->statusTip()) {
                        QRect rect = iter->path.boundingRect().toRect();
                        m_text->setReadOnly(true);
                        m_text->setText(iter->text);
                        m_text->setStatusTip(ptr);
                        m_label->setText(iter->score >= 0 ? QString("%1%").arg(iter->score, 0, 'f', 0) : "");
                        m_widget->setFixedWidth(std::max(95, rect.width()));
                        m_widget->show();
                        if (m_max_offset > 0) {
                            QPoint bottomLeft = rect.bottomLeft();
                            bottomLeft.ry() -= m_offsetY;
                            m_widget->move(mapToGlobal(bottomLeft * m_scale_ratio));
                        } else {
                            m_widget->move(mapToGlobal(rect.bottomLeft() * m_scale_ratio));
                        }
                    }
                    hide = false;
                    break;
                }
            }
            if (hide && ! m_widget->geometry().contains(gpos)) {
                hideWidget();
            }
        }
#endif
    } else if (m_cursor == Qt::BitmapCursor) {
        const QRect &rect = this->rect();
        if (m_shape != nullptr && rect.isValid()) {
            QPoint point = event->pos();
            if (rect.contains(point)) {
                m_shape->addPoint(QPoint{static_cast<int>(point.x() / m_scale_ratio), static_cast<int>(point.y() / m_scale_ratio + m_offsetY)});
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
                m_shape->addPoint(QPoint{static_cast<int>(point.x() / m_scale_ratio), static_cast<int>(point.y() / m_scale_ratio + m_offsetY)});
            }
        }
        update();
    } else if (m_cursor == Qt::SizeAllCursor && m_press &&
               ! (m_lock_pos && m_lock_pos->isChecked())) {
        if (m_move_shape) {
            QPoint point = event->pos() / m_scale_ratio;
            m_move_shape->movePoint({point.x(), point.y() + m_offsetY});
            update();
        } else {
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
            move(gpos - m_point);
#endif
        }
    }
}

void TopWidget::moveEvent(QMoveEvent *event) {
    Q_UNUSED(event);
    QString text = QString("(%1,%2 %3x%4)").arg(x()).arg(y()).arg(m_image.width()).arg(m_image.height());
    m_menu->setTitle(text);
}

void TopWidget::wheelEvent(QWheelEvent *event) {
    if (event->modifiers() == Qt::ControlModifier) {
        if (m_scale_ctrl) {
            scaleWidget(event->angleDelta().y());
        } else if (m_max_offset) {
            scrollWidget(event->angleDelta().y());
        } else {
            BaseWindow::wheelEvent(event);
        }
    } else {
        if (! m_scale_ctrl) {
            scaleWidget(event->angleDelta().y());
        } else if (m_max_offset) {
            scrollWidget(event->angleDelta().y());
        } else {
            BaseWindow::wheelEvent(event);
        }
    }
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
#ifdef OCR
    if (! m_ocr.isEmpty()) {
        m_ocr.clear();
    }
#endif
    QPainter painter(&m_image);
    if (! m_path.isEmpty()) {
        painter.setClipPath(m_path);
    }
    for (auto iter = m_vector.begin(); iter != m_vector.end(); ++iter) {
        (*iter)->scale(m_ratio, m_ratio);
        (*iter)->draw(painter);
    }
    if (m_shape != nullptr) {
        m_shape->scale(m_ratio, m_ratio);
        m_shape->draw(painter);
    }
    painter.end();
    if (path.isEmpty()) {
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

void TopWidget::updateOpacity(int value){
    double opacity = value / 100.;
    this->setWindowOpacity(opacity);
    m_tool->setWindowOpacity(opacity);
}

void TopWidget::copyImage() {
    QImage image = m_image.copy();
    QPainter painter(&image);
    if (! m_path.isEmpty()) {
        painter.setClipPath(m_path);
    }
    painter.save();
    painter.scale(m_ratio, m_ratio);
    for (auto iter = m_vector.begin(); iter != m_vector.end(); ++iter) {
        (*iter)->draw(painter);
    }
    if (m_shape != nullptr) {
        m_shape->draw(painter);
    }
    painter.restore();
    painter.end();
    QClipboard *clipboard = QApplication::clipboard();
    if (clipboard) {
        clipboard->setImage(m_image);
        addTip("复制成功");
    }
}

void TopWidget::copyWidget() {
    TopWidget *t = nullptr;
    if (m_path.elementCount() >= 2) {
        QImage image = m_image.copy();
        QVector<Shape*> v;
        t = new TopWidget{image, QPainterPath{m_path}, v, {geometry().topLeft(), m_origin}, tray_menu, m_ratio};
    } else if (m_max_offset) {
        QImage image = m_image.copy();
        t = new TopWidget{image, {geometry().topLeft(), m_origin}, tray_menu, m_ratio};
    } else {
        QVector<Shape*> v;
        t = new TopWidget{m_image.copy(), v, {geometry().topLeft(), m_origin}, tray_menu, m_ratio};
    }
    if (t && MainWindow::instance()) {
        MainWindow::instance()->connectTopWidget(t);
    }
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
    connect(m_tool, &Tool::opacityChanged, this, &TopWidget::updateOpacity);

    QString text = QString("(%1,%2 %3x%4)").arg(x()).arg(y()).arg(m_image.width()).arg(m_image.height());

    m_menu = new QMenu(this);
    m_menu->setIcon(QIcon(QPixmap::fromImage(m_image)));
    m_menu->setTitle(text);
    m_menu->addAction("显示", this, &TopWidget::moveTop);
    m_menu->addAction("关闭", this, &TopWidget::close);
    tray_menu->addMenu(m_menu);

    m_right_menu = new QMenu(this);
    m_right_menu->addAction("复制", this, &TopWidget::copyImage);
    m_right_menu->addAction("复制窗口", this, &TopWidget::copyWidget);
    m_right_menu->addAction("复制并关闭(Ctrl+C)", this, SLOT(save()));
    m_right_menu->addAction("保存到文件(Ctrl+S)", this, SIGNAL(choosePath()));
    m_right_menu->addSeparator();
    QMenu *menu1 = new QMenu{m_right_menu};
    menu1->setTitle("缩放");
    for (int i = 1; i <= 20;) {
        menu1->addAction(QString("%1%").arg(i * 25), this, [this, i] (){ scaleWidget(i * 0.25f); });
        if (i < 8) ++i;
        else i += 2;
    }
    m_right_menu->addMenu(menu1);
    QMenu *menu2 = new QMenu{m_right_menu};
    menu2->setTitle("透明度");
    for (int i = 1; i <= 10; ++i) {
        menu2->addAction(QString("%1%").arg(i * 10), this, [this, i] (){ updateOpacity(i * 10); });
    }
    m_right_menu->addMenu(menu2);
    m_disable_edit = m_right_menu->addAction("禁止移动绘制图形");
    m_disable_edit->setCheckable(true);
    m_disable_tool = m_right_menu->addAction("禁用工具栏");
    m_disable_tool->setCheckable(true);
    m_lock_pos = m_right_menu->addAction("锁定位置");
    m_lock_pos->setCheckable(true);
    m_lock_scale = m_right_menu->addAction("锁定大小");
    m_lock_scale->setCheckable(true);
    // m_right_menu->addAction("阴影");
    m_right_menu->addSeparator();
    m_right_menu->addAction("关闭", this, &TopWidget::close);

    QList<QScreen*> list = QApplication::screens();
    for (auto iter = list.cbegin(); iter != list.cend(); ++iter) {
        QRect rect = (*iter)->geometry();
        qreal ratio = (*iter)->devicePixelRatio();
        m_screen_size.setWidth(std::max<int>((rect.right() + 1 - rect.left()) * ratio + rect.left(), m_screen_size.width()));
        m_screen_size.setHeight(std::max<int>((rect.bottom() + 1 - rect.top()) * ratio + rect.top(), m_screen_size.height()));
    }

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
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);
    m_text = new QTextEdit(m_widget);
    m_text->document()->setDocumentMargin(0);
    layout->addWidget(m_text);

    QHBoxLayout *btns = new QHBoxLayout;
    btns->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    m_label = new QLabel(m_widget);
    btns->addWidget(m_label);

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

void TopWidget::scaleWidget(int delta) {
    float ratio = m_scale_ratio;
    if (delta < 0) {
        ratio -= 0.05;
        if (ratio < 0.1f) ratio = 0.1f;
    } else if (delta > 0) {
        ratio += 0.05;
        if (ratio > 5) ratio = 5;
    }
    scaleWidget(ratio);
}

void TopWidget::scaleWidget(float ratio) {
    if (ratio == m_scale_ratio || (m_lock_scale && m_lock_scale->isChecked())) return;
    m_scale_ratio = ratio;
    setFixedSize(m_origin * m_scale_ratio);
    m_tool->hide();
    if (m_ratio_timer != -1) {
        killTimer(m_ratio_timer);
    }
    m_ratio_timer = startTimer(3000);
    update();
}

void TopWidget::scrollWidget(int delta) {
    if (delta < 0) {
        m_offsetY = std::min(m_offsetY + 30, m_max_offset);
    } else if (delta > 0) {
        m_offsetY = std::max(m_offsetY - 30, 0);
    }
#ifdef OCR
    hideWidget();
#endif
    if (m_scroll_timer != -1) {
        killTimer(m_scroll_timer);
    }
    m_scroll_timer = startTimer(3000);
    update();
}

#ifdef OCR
void TopWidget::hideWidget(bool clearTip) {
    if (m_widget) {
        m_widget->hide();
        m_text->setReadOnly(true);
        if (m_button) {
            m_button->setIcon(QIcon(":/images/pencil.png"));
        }
        if (clearTip) {
            m_text->setStatusTip("");
        }
        m_text->clear();
    }
}
#endif
