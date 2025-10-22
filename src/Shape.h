#ifndef SHAPE_H
#define SHAPE_H
#include <QPainter>
#include <QPen>
#include <QPainterPath>

#define SHAPE(Class) \
public: \
    virtual Shape *getInstance(const QPoint &point, const QPen &pen, float opacity, bool fill) override { \
        return new Class(point, pen, opacity, fill); \
    }

class Shape {
public:
    Shape(const QPen &pen, float opacity = 1.0f, bool fill = false);
    virtual ~Shape() = default;
    virtual Shape *getInstance(const QPoint &point, const QPen &pen, float opacity = 1.0f, bool fill = false) = 0;
    virtual void addPoint(const QPoint &point) = 0;
    virtual bool isNull() = 0;
    virtual void scale(qreal sx, qreal sy) = 0;
    virtual void translate(const QPoint &point) = 0;
    virtual bool canMove(const QPoint&) { return false; }
    virtual void movePoint(const QPoint&) {}
    virtual void moveEnd() {}
    void translate(int x, int y);
    void draw(QPainter &painter);
    void setOpacity(float opacity);
    float opacity() const;
    void setFill(bool fill);
    bool fill() const;
    const QPen &pen() const;

protected:
    virtual void paint(QPainter &painter) = 0;
    QPen m_pen;
    bool m_fill;
    float m_opacity;
};

class Rectangle: public Shape {
    enum class Edge { None, Left, Right, Top, Bottom };
    enum class Target { None, P1, P2 };
    SHAPE(Rectangle)
public:
    Rectangle(const QPoint &point, const QPen &pen, float opacity = 1.0f, bool fill = false);
    virtual void addPoint(const QPoint &point) override;
    virtual bool isNull() override;
    virtual void scale(qreal sx, qreal sy) override;
    virtual void translate(const QPoint &point) override;
    virtual bool canMove(const QPoint &point) override;
    virtual void movePoint(const QPoint &point) override;
    virtual void moveEnd() override;

protected:
    virtual void paint(QPainter &painter) override;

private:
    QPoint p1, p2;
    Edge m_select_edge = Edge::None;
    Target m_target = Target::None;
};

class Ellipse: public Shape {
    enum class Edge { None, Left, Right, Top, Bottom };
    enum class Target { None, P1, P2 };
    SHAPE(Ellipse)
public:
    Ellipse(const QPoint &point, const QPen &pen, float opacity = 1.0f, bool fill = false);
    virtual void addPoint(const QPoint &point) override;
    virtual bool isNull() override;
    virtual void scale(qreal sx, qreal sy) override;
    virtual void translate(const QPoint &point) override;
    virtual bool canMove(const QPoint &point) override;
    virtual void movePoint(const QPoint &point) override;
    virtual void moveEnd() override;

protected:
    virtual void paint(QPainter &painter) override;

private:
    QPoint p1, p2;
    Edge m_select_edge = Edge::None;
    Target m_target = Target::None;
};

class StraightLine: public Shape {
    enum class Target { None, P1, P2 };
    SHAPE(StraightLine)
public:
    StraightLine(const QPoint &point, const QPen &pen, float opacity = 1.0f, bool fill = false);
    virtual void addPoint(const QPoint &point) override;
    virtual bool isNull() override;
    virtual void scale(qreal sx, qreal sy) override;
    virtual void translate(const QPoint &point) override;
    virtual bool canMove(const QPoint &point) override;
    virtual void movePoint(const QPoint &point) override;
    virtual void moveEnd() override;

protected:
    virtual void paint(QPainter &painter) override;

private:
    QPoint p1, p2;
    Target m_target = Target::None;
};

class Line: public Shape {
    SHAPE(Line)
public:
    Line(const QPoint &point, const QPen &pen, float opacity = 1.0f, bool fill = false);
    virtual void addPoint(const QPoint &point) override;
    virtual bool isNull() override;
    virtual void scale(qreal sx, qreal sy) override;
    virtual void translate(const QPoint &point) override;
    virtual bool canMove(const QPoint &point) override;
    virtual void movePoint(const QPoint &point) override;
    virtual void moveEnd() override;

protected:
    virtual void paint(QPainter &painter) override;

private:
    QPainterPath m_path;
    bool m_move = false;
    QPoint m_move_pos;
};

class Arrow: public Shape {
    enum class Target { None, P1, P2 };
    SHAPE(Arrow)
public:
    Arrow(const QPoint &point, const QPen &pen, float opacity = 1.0f, bool fill = false);
    virtual void addPoint(const QPoint &point) override;
    virtual bool isNull() override;
    virtual void scale(qreal sx, qreal sy) override;
    virtual void translate(const QPoint &point) override;
    virtual bool canMove(const QPoint &point) override;
    virtual void movePoint(const QPoint &point) override;
    virtual void moveEnd() override;

protected:
    virtual void paint(QPainter &painter) override;

private:
    QPoint m_p1, m_p2;
    QPainterPath m_path;
    QPen m_pen2;
    Target m_target = Target::None;
};

class Text: public Shape {
    SHAPE(Text)
public:
    Text(const QPoint &point, const QPen &pen, float opacity = 1.0f, bool fill = false);
    Text(const QPoint &point, const QPen &pen, const QFont &font, float opacity = 1.0f, bool fill = false);
    virtual void addPoint(const QPoint &point) override;
    virtual bool isNull() override;
    virtual void scale(qreal sx, qreal sy) override;
    virtual void translate(const QPoint &point) override;
    virtual bool canMove(const QPoint &point) override;
    virtual void movePoint(const QPoint &point) override;
    virtual void moveEnd() override;
    void setText(const QString &text);
    const QFont &font() const;

protected:
    virtual void paint(QPainter &painter) override;

private:
    QFont m_font;
    QPoint m_point;
    QString m_text;
    bool m_move = false;
    QPoint m_move_pos;
};

#endif // SHAPE_H
