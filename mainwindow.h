#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QSystemTrayIcon>
#include <QMenu>

#include "HotKeyWidget.h"
#include "BaseWindow.h"
#ifdef Q_OS_LINUX
#include "KeyMouseEvent.h"
#endif
#ifdef Q_OS_WINDOWS
#include <windows.h>
#include <dwmapi.h>
#endif

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
    Q_FLAG(States)
    Q_ENUM(State)
protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void paintEvent(QPaintEvent *event);
    void closeEvent(QCloseEvent *event);
#ifdef Q_OS_WINDOWS
    bool nativeEvent(const QByteArray &eventType, void *message, long *result);
#endif
    void start();
    void showTool();
    bool isValid();
private slots:
#ifdef Q_OS_LINUX
    void keyPress(int code, Qt::KeyboardModifiers modifiers);
#endif
    void updateHotkey();
    void updateCapture();
    void updateRecord();
    void quit();
    void save(const QString &path="");
    void end();
    void top();
private:
    void initHotKey();
    void saveHotKey();
    bool contains(const QPoint &point);
#ifdef Q_OS_WINDOWS
    void updateWindows();
    void addRect(HWND hwnd);
    QVector<QRect> m_windows;
    int m_index;
#endif

#ifdef Q_OS_LINUX
    KeyMouseEvent *m_monitor;
#endif
    QSystemTrayIcon *m_tray;
    QMenu *m_menu;
    States m_state;
    QImage m_gray_image;
    bool m_gif;

    QAction *m_action1;
    QAction *m_action2;
    HotKeyWidget *m_hotkey;
    HotKey m_capture;
    HotKey m_record;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(MainWindow::States)
#endif // MAINWINDOW_H
