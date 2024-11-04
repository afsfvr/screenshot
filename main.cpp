#include <QApplication>
#include <QStandardPaths>
#include <QLockFile>
#include <csignal>

#include "mainwindow.h"

#ifdef Q_OS_LINUX
void signal_handler(int x) {
    switch (x) {
    case SIGINT:  qInfo() << QString("收到信号SIGINT，退出程序");    break;
    case SIGQUIT: qInfo() << QString("收到信号SIGQUIT，退出程序");   break;
    case SIGTERM: qInfo() << QString("收到信号SIGTERM，退出程序");   break;
    default:      qInfo() << QString("收到信号%1，退出程序").arg(x); break;
    }

    QApplication::quit();
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
            qInfo() << "无法获取锁，因为另一个进程持有它";
            break;
        case QLockFile::PermissionError:
            qInfo() << "由于在父目录中没有权限，无法创建文件";
            break;
        case QLockFile::UnknownError:
            qInfo() << "另一个错误发生了，例如，一个完整的分区阻止写出文件";
            break;
        default:
            qInfo() << "未知错误";
            break;
        }

        return 2;
    }


    QApplication a(argc, argv);
#ifdef Q_OS_LINUX
    if (QApplication::platformName() != "xcb") {
        qCritical() << "不支持xcb";
        return 1;
    }
    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);
    signal(SIGTERM, signal_handler);
#endif

    QApplication::setQuitOnLastWindowClosed(false);
    MainWindow w;
    return a.exec();
}
