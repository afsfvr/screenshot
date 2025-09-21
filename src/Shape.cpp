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
