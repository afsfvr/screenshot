#if defined(_WIN32) || defined(_WIN64)
#define _USE_MATH_DEFINES
#endif
#include "Shape.h"

#include <cmath>

Shape::Shape(const QPen &pen, float opacity, bool fill): m_pen(pen), m_fill{fill} {
    setOpacity(opacity);
}

void Shape::translate(int x, int y) {
    translate(QPoint(x, y));
}

void Shape::draw(QPainter &painter) {
    if (! isNull()) {
        painter.save();
        painter.setOpacity(m_opacity);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(m_pen);
        paint(painter);
        painter.restore();
    }
}

void Shape::setOpacity(float opacity) {
    if (opacity > 1) {
        m_opacity = 1.0f;
    } else if (opacity < 0.1) {
        m_opacity = 0.1f;
    } else {
        m_opacity = opacity;
    }
}

float Shape::opacity() const {
    return m_opacity;
}

void Shape::setFill(bool fill) {
    m_fill = fill;
}

bool Shape::fill() const {
    return m_fill;
}

const QPen &Shape::pen() const {
    return m_pen;
}

Rectangle::Rectangle(const QPoint &point, const QPen &pen, float opacity, bool fill): Shape{pen, opacity, fill}, p1{point}, p2{-1, -1} {
}

void Rectangle::addPoint(const QPoint &point) {
    p2 = point;
}

bool Rectangle::isNull() {
    return (p2.x() == -1 && p2.y() == -1) || p1 == p2;
}

void Rectangle::scale(qreal sx, qreal sy) {
    if (isNull()) return;
    p1.rx() *= sx;
    p1.ry() *= sy;
    p2.rx() *= sx;
    p2.ry() *= sy;
}

void Rectangle::translate(const QPoint &point) {
    p1 += point;
    p2 += point;
}

bool Rectangle::canMove(const QPoint &point) {
    if (isNull()) return false;

    int width = m_pen.width() / 2 + 4;
    int left   = std::min(p1.x(), p2.x());
    int right  = std::max(p1.x(), p2.x());
    int top    = std::min(p1.y(), p2.y());
    int bottom = std::max(p1.y(), p2.y());

    QRect inner(left + width, top + width, right - left - 2 * width, bottom - top - 2 * width);
    QRect outer(left - width, top - width, right - left + 2 * width, bottom - top + 2 * width);
    if (! outer.contains(point)) return false;

    m_select_edge = Edge::None;
    m_target = Target::None;
    if (! inner.contains(point)) {
        if (std::abs(point.x() - left) <= width) {
            m_select_edge = Edge::Left;
            m_target = p1.x() < p2.x() ? Target::P1 : Target::P2;
        } else if (std::abs(point.x() - right) <= width) {
            m_select_edge = Edge::Right;
            m_target = p1.x() < p2.x() ? Target::P2 : Target::P1;
        } else if (std::abs(point.y() - top) <= width) {
            m_select_edge = Edge::Top;
            m_target = p1.y() < p2.y() ? Target::P1 : Target::P2;
        } else if (std::abs(point.y() - bottom) <= width) {
            m_select_edge = Edge::Bottom;
            m_target = p1.y() < p2.y() ? Target::P2 : Target::P1;
        }
        return m_select_edge != Edge::None;
    }
    return false;
}

void Rectangle::movePoint(const QPoint &point) {
    if (isNull()) return;

    QPoint *p = nullptr;
    if (m_target == Target::P1) {
        p = &p1;
    } else if (m_target == Target::P2) {
        p = &p2;
    } else {
        return;
    }
    switch (m_select_edge) {
    case Edge::Left:
    case Edge::Right:
        p->setX(point.x());
        break;
        break;
    case Edge::Top:
    case Edge::Bottom:
        p->setY(point.y());
        break;
    default:
        break;
    }
}

void Rectangle::moveEnd() {
    m_select_edge = Edge::None;
    m_target = Target::None;
}

void Rectangle::paint(QPainter &painter) {
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
    if (m_fill) {
        painter.fillRect(x, y, width, height, m_pen.color());
    } else {
        painter.drawRect(x, y, width, height);
    }
}

Ellipse::Ellipse(const QPoint &point, const QPen &pen, float opacity, bool fill): Shape{pen, opacity, fill}, p1{point}, p2{-1, -1} {
}

void Ellipse::addPoint(const QPoint &point) {
    p2 = point;
}

bool Ellipse::isNull() {
    return (p2.x() == -1 && p2.y() == -1) || p1 == p2;
}

void Ellipse::scale(qreal sx, qreal sy) {
    if (isNull()) return;
    p1.rx() *= sx;
    p1.ry() *= sy;
    p2.rx() *= sx;
    p2.ry() *= sy;
}

void Ellipse::translate(const QPoint &point) {
    p1 += point;
    p2 += point;
}

bool Ellipse::canMove(const QPoint &point) {
    if (isNull()) return false;

    int width = m_pen.width() / 2 + 4;
    int left   = std::min(p1.x(), p2.x());
    int right  = std::max(p1.x(), p2.x());
    int top    = std::min(p1.y(), p2.y());
    int bottom = std::max(p1.y(), p2.y());
    double a = (right - left) / 2.0;
    double b = (bottom - top) / 2.0;

    double dx = point.x() - (left + right) / 2;
    double dy = point.y() - (top + bottom) / 2;

    double value = (dx * dx) / (a * a) + (dy * dy) / (b * b);

    double tol = static_cast<double>(width) / std::max(a, b);

    m_select_edge = Edge::None;
    m_target = Target::None;
    if (std::abs(value - 1.0) <= tol * 2) {
        if (std::abs(point.x() - left) <= width) {
            m_select_edge = Edge::Left;
            m_target = p1.x() < p2.x() ? Target::P1 : Target::P2;
        } else if (std::abs(point.x() - right) <= width) {
            m_select_edge = Edge::Right;
            m_target = p1.x() < p2.x() ? Target::P2 : Target::P1;
        } else if (std::abs(point.y() - top) <= width) {
            m_select_edge = Edge::Top;
            m_target = p1.y() < p2.y() ? Target::P1 : Target::P2;
        } else if (std::abs(point.y() - bottom) <= width) {
            m_select_edge = Edge::Bottom;
            m_target = p1.y() < p2.y() ? Target::P2 : Target::P1;
        }

        return m_select_edge != Edge::None;
    }

    return false;
}

void Ellipse::movePoint(const QPoint &point) {
    if (isNull()) return;

    QPoint *p = nullptr;
    if (m_target == Target::P1) {
        p = &p1;
    } else if (m_target == Target::P2) {
        p = &p2;
    } else {
        return;
    }
    switch (m_select_edge) {
    case Edge::Left:
    case Edge::Right:
        p->setX(point.x());
        break;
    case Edge::Top:
    case Edge::Bottom:
        p->setY(point.y());
        break;
    default:
        break;
    }
}

void Ellipse::moveEnd() {
    m_select_edge = Edge::None;
    m_target = Target::None;
}

void Ellipse::paint(QPainter &painter) {
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
    if (m_fill) {
        painter.setClipRegion({x, y, width, height, QRegion::Ellipse});
        painter.fillRect(x, y, width, height, m_pen.color());
    } else {
        painter.drawEllipse(x, y, width, height);
    }
}

StraightLine::StraightLine(const QPoint &point, const QPen &pen, float opacity, bool fill): Shape{pen, opacity, fill}, p1{point}, p2{-1, -1} {
}

void StraightLine::addPoint(const QPoint &point) {
    p2 = point;
}

bool StraightLine::isNull() {
    return (p2.x() == -1 && p2.y() == -1) || p1 == p2;
}

void StraightLine::scale(qreal sx, qreal sy) {
    if (isNull()) return;
    p1.rx() *= sx;
    p1.ry() *= sy;
    p2.rx() *= sx;
    p2.ry() *= sy;
}

void StraightLine::translate(const QPoint &point) {
    p1 += point;
    p2 += point;
}

bool StraightLine::canMove(const QPoint &point) {
    if (isNull()) return false;

    int width = m_pen.width() / 2 + 4;
    double dx = p2.x() - p1.x();
    double dy = p2.y() - p1.y();
    double length = std::sqrt(dx*dx + dy*dy);
    if(length == 0) return false;

    double nx = -dy / length;
    double ny = dx / length;

    double halfWidth = width / 2.0;
    QPainterPath path;
    path.moveTo(p1.x() + nx * halfWidth, p1.y() + ny * halfWidth);
    path.lineTo(p2.x() + nx * halfWidth, p2.y() + ny * halfWidth);
    path.lineTo(p2.x() - nx * halfWidth, p2.y() - ny * halfWidth);
    path.lineTo(p1.x() - nx * halfWidth, p1.y() - ny * halfWidth);
    path.closeSubpath();
    if (! path.contains(point)) return false;

    double dist1 = std::hypot(point.x() - p1.x(), point.y() - p1.y());
    double dist2 = std::hypot(point.x() - p2.x(), point.y() - p2.y());

    m_target = dist1 < dist2 ? Target::P1 : Target::P2;
    return true;
}

void StraightLine::movePoint(const QPoint &point) {
    if (isNull()) return;

    switch (m_target) {
    case Target::P1:
        p1 = point;
        break;
    case Target::P2:
        p2 = point;
        break;
    default:
        break;
    }
}

void StraightLine::moveEnd() {
    m_target = Target::None;
}

void StraightLine::paint(QPainter &painter) {
    painter.drawLine(p1, p2);
}

Line::Line(const QPoint &point, const QPen &pen, float opacity, bool fill): Shape{pen, opacity, fill} {
    m_path.moveTo(point);
}

void Line::addPoint(const QPoint &point) {
    m_path.lineTo(point);
}

bool Line::isNull() {
    return m_path.elementCount() <= 2;
}

void Line::scale(qreal sx, qreal sy) {
    if (isNull()) return;
    QTransform transform;
    transform.scale(sx, sy);
    m_path = transform.map(m_path);
}

void Line::translate(const QPoint &point) {
    m_path.translate(point);
}

bool Line::canMove(const QPoint &point) {
    if (isNull()) return false;

    QPainterPathStroker stroker;
    stroker.setWidth(m_pen.width() + 4);

    if (stroker.createStroke(m_path).contains(point)) {
        m_move = true;
        m_move_pos = point;
        return true;
    }

    m_move = false;
    return false;
}

void Line::movePoint(const QPoint &point) {
    if (! m_move || isNull()) return;

    QPoint delta = point - m_move_pos;
    translate(delta);
    m_move_pos = point;
}

void Line::moveEnd() {
    m_move = false;
}

void Line::paint(QPainter &painter) {
    if (m_fill) {
        painter.fillPath(m_path, m_pen.color());
    } else {
        painter.drawPath(m_path);
    }
}

Arrow::Arrow(const QPoint &point, const QPen &pen, float opacity, bool fill): Shape{pen, opacity, fill}, m_p1{point}, m_pen2{pen} {
    m_pen2.setWidth(2);
}

void Arrow::addPoint(const QPoint &point) {
    m_p2 = point;
    m_path.clear();
}

bool Arrow::isNull() {
    return m_p2.isNull() || m_p1.isNull();
}

void Arrow::scale(qreal sx, qreal sy) {
    if (isNull()) return;
    m_p1.rx() *= sx;
    m_p1.ry() *= sy;
    m_p2.rx() *= sx;
    m_p2.ry() *= sy;
    QTransform transform;
    transform.scale(sx, sy);
    m_path = transform.map(m_path);
}

void Arrow::translate(const QPoint &point) {
    m_p2 += point;
    m_p1 += point;
    m_path.clear();
}

bool Arrow::canMove(const QPoint &point) {
    if (isNull() || ! m_path.contains(point)) return false;

    double dist1 = std::hypot(point.x() - m_p1.x(), point.y() - m_p1.y());
    double dist2 = std::hypot(point.x() - m_p2.x(), point.y() - m_p2.y());

    m_target = dist1 < dist2 ? Target::P1 : Target::P2;
    return true;
}

void Arrow::movePoint(const QPoint &point) {
    if (isNull()) return;

    switch (m_target) {
    case Target::P1:
        m_p1 = point;
        m_path.clear();
        break;
    case Target::P2:
        m_p2 = point;
        m_path.clear();
        break;
    default:
        break;
    }
}

void Arrow::moveEnd() {
    m_target = Target::None;
}

void Arrow::paint(QPainter &painter) {
    if (m_path.isEmpty()) {
        double distance = std::sqrt(std::pow(m_p2.x() - m_p1.x(), 2) + std::pow(m_p2.y() - m_p1.y(), 2));
        // 计算箭头的方向
        QLine line(m_p1, m_p2);
        double angle = std::atan2(-line.dy(), line.dx()); // 计算角度
        double arrowSize = 0.1 * m_pen.width() * std::min(distance < 40 ? distance / 4 : std::max(distance / 8, 10.0), 30.); // 箭头大小

        // 计算箭头的两个边
        QPointF arrow1 = m_p2 + QPointF(- std::sin(angle + M_PI / 3) * arrowSize,
                                       - std::cos(angle + M_PI / 3) * arrowSize);
        QPointF arrow2 = m_p2 + QPointF(std::sin(angle - M_PI / 3) * arrowSize,
                                       std::cos(angle - M_PI / 3) * arrowSize);

        QPoint arrow11((3 * arrow1.x() + arrow2.x()) / 4, (3 * arrow1.y() + arrow2.y()) / 4);
        QPoint arrow22((arrow1.x() + 3 * arrow2.x()) / 4, (arrow1.y() + 3 * arrow2.y()) / 4);

        m_path.moveTo(m_p1);
        m_path.lineTo(arrow11);
        m_path.lineTo(arrow1);
        m_path.lineTo(m_p2);
        m_path.lineTo(arrow2);
        m_path.lineTo(arrow22);
        m_path.closeSubpath();
    }
    if (m_fill) {
        painter.fillPath(m_path, m_pen.color());
    } else {
        painter.setPen(m_pen2);
        painter.drawPath(m_path);
    }
}

Text::Text(const QPoint &point, const QPen &pen, float opacity, bool fill): Shape{pen, opacity, fill}, m_point{point} {
    m_font.setPixelSize(pen.width());
    QFontMetrics metrics{m_font};
    m_point.ry() += metrics.ascent();
}

Text::Text(const QPoint &point, const QPen &pen, const QFont &font, float opacity, bool fill): Text{point, pen, opacity, fill} {
    m_font = font;
    m_font.setPixelSize(pen.width());
}

void Text::addPoint(const QPoint &point) {
    Q_UNUSED(point);
}

bool Text::isNull() {
    return (! m_text.isNull() && m_text.isEmpty());
}

void Text::scale(qreal sx, qreal sy) {
    if (isNull()) return;
    m_point.rx() *= sx;
    m_point.ry() *= sy;
}

void Text::translate(const QPoint &point) {
    m_point += point;
}

bool Text::canMove(const QPoint &point) {
    if (m_text.isEmpty()) return false;

    QFontMetrics metrics{m_font};
    QRect textRect = metrics.boundingRect(m_text);
    textRect.moveTopLeft(m_point - QPoint(0, metrics.ascent()));

    QRect hitRect = textRect.adjusted(-3, -3, 3, 3);

    if (hitRect.contains(point)) {
        m_move = true;
        m_move_pos = point;
        return true;
    }

    m_move = false;
    return false;
}

void Text::movePoint(const QPoint &point) {
    if (!m_move || m_text.isEmpty()) return;

    QPoint delta = point - m_move_pos;
    translate(delta);
    m_move_pos = point;
}

void Text::moveEnd() {
    m_move = false;
}

void Text::setText(const QString &text) {
    m_text = text;
}

const QFont &Text::font() const {
    return m_font;
}

void Text::paint(QPainter &painter) {
    painter.setFont(m_font);
    painter.drawText(m_point, m_text);
}
