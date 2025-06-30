#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QSystemTrayIcon>
#include <QMenu>

#include "SettingWidget.h"
#include "BaseWindow.h"
#ifdef Q_OS_LINUX
#include "KeyMouseEvent.h"
#endif
#ifdef Q_OS_WINDOWS
#include <windows.h>
#include <dwmapi.h>
#endif

class TopWidget;
class MainWindow : public BaseWindow
{
    Q_OBJECT

public:
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

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
#ifdef Q_OS_WINDOWS
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#else
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;
#endif
#endif
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
#endif
#ifdef OCR
    void ocrStart();
#endif
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
    void openSaveDir();
    bool contains(const QPoint &point);
    void updateWindows();
#ifdef Q_OS_LINUX
    QString getWindowTitle(Display *display, Window window);
#endif
#ifdef Q_OS_WINDOWS
    QRect getRectByHwnd(HWND hwnd);
#endif

#ifdef Q_OS_LINUX
    KeyMouseEvent *m_monitor;
#endif
    QVector<QRect> m_windows;
    int m_index;
    QSystemTrayIcon *m_tray;
    QMenu *m_menu;
    States m_state;
    ResizeImages m_resize;
    QImage m_gray_image;
    bool m_gif;

    QAction *m_action1;
    QAction *m_action2;
    QAction *m_action3;
    SettingWidget *m_setting;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(MainWindow::States)
Q_DECLARE_OPERATORS_FOR_FLAGS(MainWindow::ResizeImages)
#endif // MAINWINDOW_H
