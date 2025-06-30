#include <QApplication>

#include "KeyMouseEvent.h"

#define Button1			1
#define Button2			2
#define Button3			3
#define WheelUp			4
#define WheelDown		5
#define WheelLeft		6
#define WheelRight		7

KeyMouseEvent::KeyMouseEvent(QObject *parent)
    : QThread{parent}, running{false}
{
    qRegisterMetaType<QSharedPointer<QMouseEvent>>("QSharedPointer<QMouseEvent>");
    qRegisterMetaType<QSharedPointer<QWheelEvent>>("QSharedPointer<QWheelEvent>");
    qRegisterMetaType<Qt::KeyboardModifiers>("Qt::KeyboardModifiers");
    moveToThread(this);
}

KeyMouseEvent::~KeyMouseEvent() {
    running = false;
    quit();
    wait();
}

void KeyMouseEvent::callback(XPointer ptr, XRecordInterceptData *data) {
    ((KeyMouseEvent *) ptr)->handleEvent(data);
}

void KeyMouseEvent::resume() {
    if (! running) {
        running = true;
        QMetaObject::invokeMethod(this, "eventLoop", Qt::QueuedConnection);
    }
}

void KeyMouseEvent::pause() {
    running = false;
}

void KeyMouseEvent::run() {
    display = XOpenDisplay(nullptr);
    if (display == nullptr) {
        fprintf(stderr, "unable to open display\n");
        return;
    }

    XRecordRange* range = XRecordAllocRange();
    if (range == nullptr) {
        XCloseDisplay(display);
        fprintf(stderr, "unable to allocate XRecordRange\n");
        return;
    }
    memset(range, 0, sizeof(XRecordRange));
    range->device_events.first = KeyPress;
    range->device_events.last  = MotionNotify;

    XRecordClientSpec clients = XRecordAllClients;
    context = XRecordCreateContext (display, 0, &clients, 1, &range, 1);
    if (context == 0) {
        XFree(range);
        XCloseDisplay(display);
        fprintf(stderr, "XRecordCreateContext failed\n");
        return;
    }

    XFree(range);

    XSync(display, True);

    if (! XRecordEnableContextAsync(display, context,  callback, (XPointer) this)) {
        XRecordFreeContext(display, context);
        XCloseDisplay(display);
        fprintf(stderr, "XRecordEnableContext() failed\n");
        return;
    }
    exec();
    XRecordDisableContext(display, context);
    XRecordFreeContext(display, context);
    XFree(display);
}

void KeyMouseEvent::eventLoop() {
    while (running) {
        XRecordProcessReplies(display);
        QThread::msleep(100);
    }
}

void KeyMouseEvent::handleEvent(XRecordInterceptData *data) {
    if (data->category == XRecordFromServer) {
        xEvent * event = (xEvent *)data->data;
        QPointF point = QPointF(event->u.keyButtonPointer.rootX, event->u.keyButtonPointer.rootY);
        switch (event->u.u.type) {
        case ButtonPress:
            switch (event->u.u.detail) {
            case Button1:
                m_buttons |= Qt::LeftButton;
                emit mousePress(QSharedPointer<QMouseEvent>::create(QEvent::MouseButtonPress, point, point, point, Qt::LeftButton, m_buttons, m_modifiers, Qt::MouseEventSynthesizedByApplication));
                break;
            case Button2:
                m_buttons |= Qt::MiddleButton;
                emit mousePress(QSharedPointer<QMouseEvent>::create(QEvent::MouseButtonPress, point, point, point, Qt::MiddleButton, m_buttons, m_modifiers, Qt::MouseEventSynthesizedByApplication));
                break;
            case Button3:
                m_buttons |= Qt::RightButton;
                emit mousePress(QSharedPointer<QMouseEvent>::create(QEvent::MouseButtonPress, point, point, point, Qt::RightButton, m_buttons, m_modifiers, Qt::MouseEventSynthesizedByApplication));
                break;
            case WheelUp:
                emit mouseWheel(QSharedPointer<QWheelEvent>::create(point, point, QPoint{0, 120}, QPoint{0, 120}, m_buttons, m_modifiers, Qt::NoScrollPhase, false, Qt::MouseEventSynthesizedByApplication));
                break;
            case WheelDown:
                emit mouseWheel(QSharedPointer<QWheelEvent>::create(point, point, QPoint{0, -120}, QPoint{0, -120}, m_buttons, m_modifiers, Qt::NoScrollPhase, false, Qt::MouseEventSynthesizedByApplication));
                break;
            case WheelLeft:
            case WheelRight:
                break;
            }
            break;
        case MotionNotify: {
            auto ptr = QSharedPointer<QMouseEvent>::create(QEvent::MouseMove, point, point, point, Qt::NoButton, m_buttons, m_modifiers, Qt::MouseEventSynthesizedByApplication);
            if (m_buttons != Qt::NoButton) {
                emit mousePressMove(ptr);
            }
            emit mouseMove(ptr);
            break;
        }
        case ButtonRelease:
            switch (event->u.u.detail) {
            case Button1:
                m_buttons &= ~Qt::LeftButton;
                emit mouseRelease(QSharedPointer<QMouseEvent>::create(QEvent::MouseButtonRelease, point, point, point, Qt::LeftButton, m_buttons, m_modifiers, Qt::MouseEventSynthesizedByApplication));
                break;
            case Button2:
                m_buttons &= ~Qt::MiddleButton;
                emit mouseRelease(QSharedPointer<QMouseEvent>::create(QEvent::MouseButtonRelease, point, point, point, Qt::MiddleButton, m_buttons, m_modifiers, Qt::MouseEventSynthesizedByApplication));
                break;
            case Button3:
                m_buttons &= ~Qt::RightButton;
                emit mouseRelease(QSharedPointer<QMouseEvent>::create(QEvent::MouseButtonRelease, point, point, point, Qt::RightButton, m_buttons, m_modifiers, Qt::MouseEventSynthesizedByApplication));
                break;
            case WheelUp:
            case WheelDown:
            case WheelLeft:
            case WheelRight:
                break;
            }
            break;
        case KeyPress:
            if (event->u.u.detail == 37 || event->u.u.detail == 105) {
                m_modifiers |= Qt::ControlModifier;
            }
            if (event->u.u.detail == 64 || event->u.u.detail == 108) {
                m_modifiers |= Qt::AltModifier;
            }
            if (event->u.u.detail == 50 || event->u.u.detail == 62) {
                m_modifiers |= Qt::ShiftModifier;
            }
            emit keyPress(event->u.u.detail, m_modifiers);
            break;
        case KeyRelease:
            if (event->u.u.detail == 37 || event->u.u.detail == 105) {
                m_modifiers &= ~Qt::ControlModifier;
            }
            if (event->u.u.detail == 64 || event->u.u.detail == 108) {
                m_modifiers &= ~Qt::AltModifier;
            }
            if (event->u.u.detail == 50 || event->u.u.detail == 62) {
                m_modifiers &= ~Qt::ShiftModifier;
            }
            emit keyRelease(event->u.u.detail, m_modifiers);
            break;
        default:
            break;
        }
    }

    // 资源释放
    XRecordFreeData(data);
}
