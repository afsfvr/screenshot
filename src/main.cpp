#include <QApplication>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QLockFile>
#include <csignal>

#include "mainwindow.h"
#include "TopWidget.h"

#if defined(Q_OS_LINUX)
void signal_handler(int x) {
    switch (x) {
    case SIGINT:  qInfo() << QString("收到信号SIGINT，退出程序");    break;
    case SIGQUIT: qInfo() << QString("收到信号SIGQUIT，退出程序");   break;
    case SIGTERM: qInfo() << QString("收到信号SIGTERM，退出程序");   break;
    default:      qInfo() << QString("收到信号%1，退出程序").arg(x); break;
    }

    QApplication::quit();
}
#elif defined(Q_OS_WINDOWS)
static HHOOK hHook;
static HWND targetHwnd = nullptr;
static LRESULT CALLBACK mouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (wParam == WM_MOUSEWHEEL && targetHwnd != nullptr) {
        MSLLHOOKSTRUCT * msll = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        int delta = GET_WHEEL_DELTA_WPARAM(msll->mouseData);
        PostMessageW(targetHwnd, WM_USER + 1024, delta <= 0, 0);
    }
    return CallNextHookEx(hHook, nCode, wParam, lParam);
}
#endif

int main(int argc, char *argv[])
{
#if ! defined(Q_OS_LINUX) && ! defined(Q_OS_WINDOWS)
#error "unsupported platform"
#endif
    const QString &tmpFile = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QLockFile file(tmpFile + "/61fd13c5-539c-4db3-8acd-139f0e9a6beb");
    if (! file.tryLock()) {
        switch (file.error()) {
        case QLockFile::LockFailedError:
            qWarning() << "无法获取锁，因为另一个进程持有它";
            break;
        case QLockFile::PermissionError:
            qWarning() << "由于在父目录中没有权限，无法创建文件";
            break;
        case QLockFile::UnknownError:
            qWarning() << "另一个错误发生了，例如，一个完整的分区阻止写出文件";
            break;
        default:
            qWarning() << "未知错误";
            break;
        }

        return 2;
    }

#ifdef Q_OS_LINUX
    if (QProcessEnvironment::systemEnvironment().value("DISPLAY").isEmpty()) {
        qCritical() << "无图形环境";
        return 1;
    }
#endif
    QApplication a(argc, argv);
#ifdef Q_OS_LINUX
    if (QApplication::platformName() != "xcb" || qgetenv("XDG_SESSION_TYPE") != "x11") {
        qCritical() << "不是Xorg";
        return 1;
    }
    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);
    signal(SIGTERM, signal_handler);
#endif

    qRegisterMetaType<TopWidget*>("TopWidget*");
    QApplication::setWindowIcon(QIcon(":/images/screenshot.ico"));
    QApplication::setQuitOnLastWindowClosed(false);
    MainWindow w;
#ifdef Q_OS_WINDOWS
    hHook = SetWindowsHookExW(WH_MOUSE_LL, mouseProc, NULL, 0);
    targetHwnd = reinterpret_cast<HWND>(w.winId());
#endif
#ifdef OCR
    qRegisterMetaType<QVector<Ocr::OcrResult>>("QVector<Ocr::OcrResult>");
    OcrInstance->start();
#endif
    int ret = a.exec();
#ifdef OCR
    OcrInstance->quit();
    OcrInstance->wait();
#endif
#ifdef Q_OS_WINDOWS
    targetHwnd = nullptr;
    UnhookWindowsHookEx(hHook);
#endif
    return ret;
}
