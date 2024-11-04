#ifndef TOOL_H
#define TOOL_H

#include <QWidget>
#include <QPen>
#include <QFileDialog>
#include <QColorDialog>

#include "Shape.h"
namespace Ui {
class Tool;
}

class Tool : public QWidget
{
    Q_OBJECT

    enum class ShapeEnum {
        Null,
        Rectangle,
        Ellipse,
        StraightLine,
        Line,
        Arrow
    };

public:
    explicit Tool(QWidget *parent = nullptr);
    ~Tool();
    bool isDraw();
    Shape *getShape(const QPoint &point);
    void showEvent(QShowEvent *event);
public slots:
    void choosePath();
    void penChange(int value = -1);
    void setDraw(ShapeEnum shape);
    void topChange();
signals:
    void save(const QString &path = "");
    void cancel();
    void penChanged(const QPen &pen, bool draw);
    void clickTop();
    void topChanged(bool top);
private:
    Ui::Tool *ui;
    QPen m_pen;
    Shape *m_shape;
    static QString savePath;
};

#endif // TOOL_H
