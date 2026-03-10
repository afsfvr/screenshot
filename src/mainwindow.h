#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QSystemTrayIcon>
#include <QMenu>

#include "SettingWidget.h"
#include "BaseWindow.h"
#if defined(Q_OS_LINUX)
#include <QX11Info>
#include <QAbstractNativeEventFilter>
#include <xcb/xcb.h>
#include "KeyMouseEvent.h"
#elif defined(Q_OS_WINDOWS)
#include <windows.h>
#include <dwmapi.h>
#endif

class TopWidget;
class MainWindow : public BaseWindow
#ifdef Q_OS_LINUX
    , public QAbstractNativeEventFilter
#endif // Q_OS_LINUX
{
    Q_OBJECT

public:
    static MainWindow *instance();
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void connectTopWidget(TopWidget *t);
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

    enum ResizeImage {
        NoResize   = 0,
        Top        = 1,
        Left       = 2,
        Right      = 4,
        Bottom     = 8,
        TopLeft    = Top | Left,
        TopRight   = Top | Right,
        BottomLeft = Bottom | Left,
        BottomRight= Bottom | Right
    };
    Q_DECLARE_FLAGS(ResizeImages, ResizeImage)
    Q_FLAG(ResizeImages)
    Q_ENUM(ResizeImage)

#ifdef Q_OS_LINUX
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
#else
    bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) override;
#endif // QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#endif // Q_OS_LINUX

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
#ifdef Q_OS_WINDOWS
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#else
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;
#endif // QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#endif // Q_OS_WINDOWS
    void saveImage();
    void start();
    void gifStart();
    void showTool();
    bool isValid() const override;
    QRect getGeometry() const override;

signals:
    void started();
    void finished();
    void mouseWheeled(bool down);

private slots:
#ifdef Q_OS_LINUX
    void keyPress(int code, Qt::KeyboardModifiers modifiers);
    void mouseWheel(QSharedPointer<QWheelEvent> event);
    void grabMouseEvent();
#endif // Q_OS_LINUX
#ifdef OCR
    void ocrStart();
#endif // OCR
    void longScreenshot();
    void updateHotkey();
    void updateAutoSave(const HotKey &key, quint8 mode, const QString &path);
    void updateCapture(const HotKey &key);
    void updateRecord(const HotKey &key);
    void quit();
    void save(const QString &path="") override;
    void end() override;
    TopWidget *top();
private:
    void initTray();
    void openSaveDir();
    bool contains(const QPoint &point);
    void updateWindows();
    QImage fullScreenshot();
#ifdef Q_OS_LINUX
    QString getWindowTitle(Display *display, Window window);
    bool UnregisterHotKey(const HotKey &key);
    bool RegisterHotKey(HotKey &key);
    static int handleError(Display *display, XErrorEvent *error);
#endif // Q_OS_LINUX
#ifdef Q_OS_WINDOWS
    QRect getRectByHwnd(HWND hwnd);
#endif // Q_OS_WINDOWS

#ifdef Q_OS_LINUX
    KeyMouseEvent *m_monitor;
    bool m_grab_mouse = false;
    HotKey m_key1;
    HotKey m_key2;
    HotKey m_key3;
    QString m_grab_error;
#endif // Q_OS_LINUX
    QVector<QRect> m_windows;
    int m_index;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_menu = nullptr;
    States m_state;
    ResizeImages m_resize;
    QImage m_gray_image;
    bool m_gif;

    QAction *m_action1 = nullptr;
    QAction *m_action2 = nullptr;
    QAction *m_action3 = nullptr;
    SettingWidget *m_setting;

    static MainWindow *self;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(MainWindow::States)
Q_DECLARE_OPERATORS_FOR_FLAGS(MainWindow::ResizeImages)
#endif // MAINWINDOW_H
