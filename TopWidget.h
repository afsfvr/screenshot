#ifndef TOPWIDGET_H
#define TOPWIDGET_H

#include <QWidget>
#include <QMenu>

#include "BaseWindow.h"

class TopWidget : public BaseWindow {
    Q_OBJECT
public:
    explicit TopWidget(QImage &&image, QVector<Shape*> &vector, const QRect &rect, QMenu *menu);
    explicit TopWidget(QImage &image, QPainterPath &&path, QVector<Shape*> &vector, const QRect &rect, QMenu *menu);
    virtual ~TopWidget();
    void showTool();

protected:
    void closeEvent(QCloseEvent *event);
    void paintEvent(QPaintEvent *event);
    void hideEvent(QHideEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void moveEvent(QMoveEvent *event);
    void focusOutEvent(QFocusEvent *event);
    inline bool isValid() { return true; }
private slots:
    void save(const QString &path="");
    void end();
    void topChange(bool top);
    void moveTop();
private:
    void init();
    bool contains(const QPoint &point);
private:
    QMenu *tray_menu;
    QMenu *m_menu;
};

#endif // TOPWIDGET_H
