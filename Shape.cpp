#include "Shape.h"

Shape::Shape(const QPen &pen): m_pen(pen) {
}

void Shape::translate(int x, int y) {
    translate(QPoint(x, y));
}

void Shape::draw(QPainter &painter) {
    if (! isNull()) {
        QPen pen = painter.pen();
        painter.setPen(m_pen);
        paint(painter);
        painter.setPen(pen);
    }
}

Rectangle::Rectangle(const QPoint &point, const QPen &pen): Shape(pen), p1(point) {
}

void Rectangle::addPoint(const QPoint &point) {
    p2 = point;
}

bool Rectangle::isNull() {
    return p2.isNull() || p1.isNull();
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

Ellipse::Ellipse(const QPoint &point, const QPen &pen): Shape(pen), p1(point) {
}

void Ellipse::addPoint(const QPoint &point) {
    p2 = point;
}

bool Ellipse::isNull() {
    return p2.isNull() || p1.isNull();
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

StraightLine::StraightLine(const QPoint &point, const QPen &pen): Shape(pen), p1(point) {
}

void StraightLine::addPoint(const QPoint &point) {
    p2 = point;
}

bool StraightLine::isNull() {
    return p2.isNull() || p1.isNull();
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
