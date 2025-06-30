#ifndef KEYMOUSEEVENT_H
#define KEYMOUSEEVENT_H

#include <QSharedPointer>
#include <QThread>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QDebug>
#include <X11/Xlibint.h>
#include <X11/Xlib.h>
#include <X11/extensions/record.h>
#include <X11/extensions/Xrandr.h>
#undef Bool

class KeyMouseEvent : public QThread
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(KeyMouseEvent)
public:
    explicit KeyMouseEvent(QObject *parent = nullptr);
    ~KeyMouseEvent();
    static void callback(XPointer ptr, XRecordInterceptData* data);
    void resume();
    void pause();
protected:
    virtual void run() override;
    Q_INVOKABLE void eventLoop();
    void handleEvent(XRecordInterceptData* data);

signals:
    void mousePress(QSharedPointer<QMouseEvent> event);
    void mouseRelease(QSharedPointer<QMouseEvent> event);
    void mousePressMove(QSharedPointer<QMouseEvent> event);
    void mouseMove(QSharedPointer<QMouseEvent> event);
    void mouseWheel(QSharedPointer<QWheelEvent> event);

    void keyPress(int code, Qt::KeyboardModifiers modifiers);
    void keyRelease(int code, Qt::KeyboardModifiers modifiers);
private:
    bool running;
    Qt::KeyboardModifiers m_modifiers;
    Qt::MouseButtons m_buttons;
    Display *display;
    XRecordContext context;
};

#endif // KEYMOUSEEVENT_H
