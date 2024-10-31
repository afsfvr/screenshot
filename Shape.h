#ifndef SHAPE_H
#define SHAPE_H

#include <QObject>
#include <QPainter>
#include <QPen>
#include <QPainterPath>

#define SHAPE(Class) \
    Q_OBJECT \
public: \
    virtual Shape *getInstance(const QPoint &point, const QPen &pen) { \
        return new Class(point, pen); \
    }

#define Q_DISABLE_COPY(Class) \
Class(const Class &) = delete;\
    Class &operator=(const Class &) = delete;
class Shape: public QObject {
    Q_OBJECT
public:
    Shape(const QPen &pen);
    virtual ~Shape() = default;
    virtual Shape *getInstance(const QPoint &point, const QPen &pen) = 0;
    virtual void addPoint(const QPoint &point) = 0;
    virtual bool isNull() = 0;
    virtual void translate(const QPoint &point) = 0;
    void translate(int x, int y);
    void draw(QPainter &painter);
protected:
    virtual void paint(QPainter &painter) = 0;
    QPen m_pen;
};

class Rectangle: public Shape {
    SHAPE(Rectangle)
public:
    Rectangle(const QPoint &point, const QPen &pen);
    virtual void addPoint(const QPoint &point);
    virtual bool isNull();
    virtual void translate(const QPoint &point);
protected:
    virtual void paint(QPainter &painter);
private:
    QPoint p1, p2;
};

class Ellipse: public Shape {
    SHAPE(Ellipse)
public:
    Ellipse(const QPoint &point, const QPen &pen);
    virtual void addPoint(const QPoint &point);
    virtual bool isNull();
    virtual void translate(const QPoint &point);
protected:
    virtual void paint(QPainter &painter);
private:
    QPoint p1, p2;
};

class StraightLine: public Shape {
    SHAPE(StraightLine)
public:
    StraightLine(const QPoint &point, const QPen &pen);
    virtual void addPoint(const QPoint &point);
    virtual bool isNull();
    virtual void translate(const QPoint &point);
protected:
    virtual void paint(QPainter &painter);
private:
    QPoint p1, p2;
};

class Line: public Shape {
    SHAPE(Line)
public:
    Line(const QPoint &point, const QPen &pen);
    virtual void addPoint(const QPoint &point);
    virtual bool isNull();
    virtual void translate(const QPoint &point);
protected:
    virtual void paint(QPainter &painter);
private:
    QPainterPath m_path;
};

class Arrow: public Shape {
    SHAPE(Arrow)
public:
    Arrow(const QPoint &point, const QPen &pen);
    virtual void addPoint(const QPoint &point);
    virtual bool isNull();
    virtual void translate(const QPoint &point);
protected:
    virtual void paint(QPainter &painter);
private:
    QPoint m_p1, m_p2;
    QPainterPath m_path;
};

#endif // SHAPE_H
