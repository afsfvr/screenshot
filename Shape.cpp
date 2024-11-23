#if defined(_WIN32) || defined(_WIN64)
#define _USE_MATH_DEFINES
#endif
#include "Shape.h"
#include <cmath>

Shape::Shape(const QPen &pen): m_pen(pen) {
}

void Shape::translate(int x, int y) {
    translate(QPoint(x, y));
}

void Shape::draw(QPainter &painter) {
    if (! isNull()) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(m_pen);
        paint(painter);
        painter.restore();
    }
}

Rectangle::Rectangle(const QPoint &point, const QPen &pen): Shape(pen), p1(point), p2(-1, -1) {
}

void Rectangle::addPoint(const QPoint &point) {
    p2 = point;
}

bool Rectangle::isNull() {
    return (p2.x() == -1 && p2.y() == -1) || p1 == p2;
}

void Rectangle::translate(const QPoint &point) {
    p1 += point;
    p2 += point;
}

void Rectangle::paint(QPainter &painter) {
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
    painter.drawRect(x, y, width, height);
}

Ellipse::Ellipse(const QPoint &point, const QPen &pen): Shape(pen), p1(point), p2(-1, -1) {
}

void Ellipse::addPoint(const QPoint &point) {
    p2 = point;
}

bool Ellipse::isNull() {
    return (p2.x() == -1 && p2.y() == -1) || p1 == p2;
}

void Ellipse::translate(const QPoint &point) {
    p1 += point;
    p2 += point;
}

void Ellipse::paint(QPainter &painter) {
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
    painter.drawEllipse(x, y, width, height);
}

StraightLine::StraightLine(const QPoint &point, const QPen &pen): Shape(pen), p1(point), p2(-1, -1) {
}

void StraightLine::addPoint(const QPoint &point) {
    p2 = point;
}

bool StraightLine::isNull() {
    return (p2.x() == -1 && p2.y() == -1) || p1 == p2;
}

void StraightLine::translate(const QPoint &point) {
    p1 += point;
    p2 += point;
}

void StraightLine::paint(QPainter &painter) {
    painter.drawLine(p1, p2);
}

Line::Line(const QPoint &point, const QPen &pen): Shape(pen) {
    m_path.moveTo(point);
}

void Line::addPoint(const QPoint &point) {
    m_path.lineTo(point);
}

bool Line::isNull() {
    return m_path.elementCount() <= 2;
}

void Line::translate(const QPoint &point) {
    m_path.translate(point);
}

void Line::paint(QPainter &painter) {
    painter.drawPath(m_path);
}

Arrow::Arrow(const QPoint &point, const QPen &pen): Shape(pen), m_p1(point) {
}

void Arrow::addPoint(const QPoint &point) {
    m_p2 = point;
    m_path.clear();
}

bool Arrow::isNull() {
    return m_p2.isNull() || m_p1.isNull();
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
    painter.fillPath(m_path, m_pen.color());
}
