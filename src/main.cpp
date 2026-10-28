#include <QApplication>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QLockFile>
#include <csignal>
#if defined(Q_OS_LINUX)
#include <QSocketNotifier>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

#include "mainwindow.h"
#include "TopWidget.h"

#if defined(Q_OS_LINUX)
static int signalPipe[2] = {-1, -1};

static void signal_handler(int x) {
    const unsigned char sig = static_cast<unsigned char>(x);
    const int savedErrno = errno;
    const ssize_t ignored = ::write(signalPipe[1], &sig, 1);
    Q_UNUSED(ignored);
    errno = savedErrno;
}
#elif defined(Q_OS_WINDOWS)
static HHOOK hHook = nullptr;
static HWND targetHwnd = nullptr;
static LRESULT CALLBACK mouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (wParam == WM_MOUSEWHEEL && targetHwnd != nullptr) {
        MSLLHOOKSTRUCT * msll = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        int delta = GET_WHEEL_DELTA_WPARAM(msll->mouseData);
        PostMessageW(targetHwnd, WM_USER + 1024, delta <= 0, 0);
    }
    return CallNextHookEx(hHook, nCode, wParam, lParam);
}

static LONG setStartup(bool startup) {
    HKEY hKey = nullptr;
    LONG result = RegCreateKeyExW(
        HKEY_LOCAL_MACHINE,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_SET_VALUE | KEY_QUERY_VALUE,
        nullptr,
        &hKey,
        nullptr
        );

    if (result != ERROR_SUCCESS) {
        return result;
    }

    if (startup) {
        wchar_t path[1027];
        DWORD len = GetModuleFileNameW(NULL, path + 1, 1024);
        path[0] = L'"';
        path[len + 1] = L'"';
        path[len + 2] = L'\0';
        len += 3;

        result = RegSetValueExW(
            hKey,
            L"screenshot",
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(path),
            static_cast<DWORD>((len) * sizeof(wchar_t))
            );

        RegCloseKey(hKey);

        if (result != ERROR_SUCCESS) {
            return result;
        }
    } else {
        result = RegDeleteValueW(hKey, L"screenshot");
        RegCloseKey(hKey);

        if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
            return result;
        }
    }

    return 0;
}
#endif

int main(int argc, char *argv[])
{
#if ! defined(Q_OS_LINUX) && ! defined(Q_OS_WINDOWS)
#error "unsupported platform"
#endif
#ifdef Q_OS_WINDOWS
    if (argc == 2) {
        if (QString::compare(argv[1], "--enable-startup") == 0) {
            return static_cast<int>(setStartup(true));
        } else if (QString::compare(argv[1], "--disable-startup") == 0) {
            return static_cast<int>(setStartup(false));
        }
    }
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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication a(argc, argv);
#ifdef Q_OS_LINUX
    if (QApplication::platformName() != "xcb" || qgetenv("XDG_SESSION_TYPE") != "x11") {
        qCritical() << "不是Xorg";
        return 1;
    }

    if (::pipe(signalPipe) != 0) {
        qCritical() << "创建信号管道失败";
        return 1;
    }
    for (int fd : signalPipe) {
        const int flags = ::fcntl(fd, F_GETFL, 0);
        if (flags != -1) {
            ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }
    }

    QSocketNotifier notifier{signalPipe[0], QSocketNotifier::Read};
    QObject::connect(&notifier, &QSocketNotifier::activated, &a, [&a]() {
        char sig = 0;
        while (::read(signalPipe[0], &sig, sizeof(sig)) == sizeof(sig)) {
            switch (sig) {
            case SIGINT:  qInfo().noquote() << "收到信号SIGINT，退出程序";  break;
            case SIGQUIT: qInfo().noquote() << "收到信号SIGQUIT，退出程序"; break;
            case SIGTERM: qInfo().noquote() << "收到信号SIGTERM，退出程序"; break;
            default:      qInfo().noquote() << QString("收到信号%1，退出程序").arg(static_cast<int>(sig)); break;
            }
        }
        a.quit();
    });

    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);
    signal(SIGTERM, signal_handler);
#endif

    qRegisterMetaType<TopWidget*>("TopWidget*");
    QApplication::setWindowIcon(QIcon(":/images/screenshot.ico"));
    QApplication::setQuitOnLastWindowClosed(false);
    MainWindow w;
#ifdef Q_OS_WINDOWS
    if (! IsDebuggerPresent()) {
        hHook = SetWindowsHookExW(WH_MOUSE_LL, mouseProc, NULL, 0);
        targetHwnd = reinterpret_cast<HWND>(w.winId());
    }
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
    if (hHook != nullptr) {
        targetHwnd = nullptr;
        UnhookWindowsHookEx(hHook);
    }
#endif
    return ret;
}
