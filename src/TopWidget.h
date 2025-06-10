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

#ifdef Q_OS_LINUX
public slots:
    void mouseRelease(QSharedPointer<QMouseEvent> event);
#endif

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    inline void mouseDoubleClickEvent(QMouseEvent *event) override { QWidget::mouseDoubleClickEvent(event); }
    void focusOutEvent(QFocusEvent *event) override;
    inline bool isValid() const override { return true; }
    QRect getGeometry() const override;
private slots:
    void save(const QString &path="") override;
    void end() override;
    void topChange(bool top);
    void moveTop();
private:
    void init();
    bool contains(const QPoint &point);
private:
    QMenu *tray_menu;
    QMenu *m_menu;
#ifdef Q_OS_LINUX
    bool m_move = false;
#endif
};

#endif // TOPWIDGET_H
