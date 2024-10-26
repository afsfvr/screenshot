#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QSystemTrayIcon>
#include <QMenu>

#include "BaseWindow.h"
#include "KeyMouseEvent.h"

class MainWindow : public BaseWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    enum State {
        Null = 0,
        Screen = 1,
        Edit = 2,
        Free = 4,
        Rect = 8,
        FreeScreen = Free | Screen,
        FreeEdit = Free | Edit,
        RectScreen = Rect | Screen,
        RectEdit = Rect | Edit
    };
    Q_DECLARE_FLAGS(States, State)
protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void paintEvent(QPaintEvent *event);
    void closeEvent(QCloseEvent *event);
    void hideEvent(QHideEvent *event);
    void start();
    void showTool();
private slots:
    void quit();
    void save(const QString &path);
private:
    bool contains(const QPoint &point);
private:
    KeyMouseEvent *m_monitor;
    QSystemTrayIcon *m_tray;
    QMenu *m_menu;
    States m_state;
    QImage m_gray_image;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(MainWindow::States)
#endif // MAINWINDOW_H
