#ifndef BASEWINDOW_H
#define BASEWINDOW_H

#include <QWidget>
#include <QApplication>
#include <QScreen>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QDebug>
#include <QVector>
#include <QStack>
#include <QClipboard>
#include <QPainterPath>
#include <QLineEdit>
#include <QDateTime>

#include "Shape.h"
#include "Tool.h"

class BaseWindow : public QWidget
{
    Q_OBJECT
public:
    explicit BaseWindow(QWidget *parent = nullptr);
    virtual ~BaseWindow();
protected:
    virtual void keyPressEvent(QKeyEvent *event);
    virtual void mouseDoubleClickEvent(QMouseEvent *event);
    virtual bool eventFilter(QObject *watched, QEvent *event);
    virtual void timerEvent(QTimerEvent *event);

    QImage fullScreenshot();
    QRect getRect(const QPoint &p1, const QPoint &p2);
    void setShape(const QPoint &point);
    virtual bool isValid() const = 0;
    virtual QRect getGeometry() const = 0;
protected slots:
    virtual void undo();
    virtual void redo();
    virtual void save(const QString &path="") = 0;
    virtual void end() = 0;
    virtual void saveColor();
    virtual int addTip(const QString &text, int duration = 2000);
    virtual bool removeTip(int id);
    virtual bool removeTip(const QString &text);
    void clearDraw();
    void clearStack();
    void drawTips(QPainter &painter);
    void setCursorShape(Qt::CursorShape cursor=Qt::ArrowCursor);
signals:
    void choosePath();
protected:
    QImage m_image;
    QVector<Shape*> m_vector;
    QStack<Shape*> m_stack;
    QPainterPath m_path;
    bool m_press;
    QPoint m_point;
    QRect m_rect;
    Shape *m_shape;
    Tool *m_tool;
    QLineEdit *m_edit;
    bool m_ignore;
    Qt::CursorShape m_cursor;

    struct TipStruct {
        int id;
        QString text;
        qint64 time;
        int duration=2000;
    };
    QVector<TipStruct> m_tips;
};

template<typename T>
void safeDelete(T *&ptr) {
    if (ptr != nullptr) {
        delete ptr;
        ptr = nullptr;
    }
}

#endif // BASEWINDOW_H
