#include <QApplication>
#include <QStandardPaths>
#include <QLockFile>
#include <csignal>

#include "mainwindow.h"

void signal_handler(int x) {
    switch (x) {
    case SIGINT:  qInfo() << QString("收到信号SIGINT，退出程序");    break;
    case SIGQUIT: qInfo() << QString("收到信号SIGQUIT，退出程序");   break;
    case SIGTERM: qInfo() << QString("收到信号SIGTERM，退出程序");   break;
    default:      qInfo() << QString("收到信号%1，退出程序").arg(x); break;
    }

    QApplication::quit();
}

int main(int argc, char *argv[])
{
#if ! defined(Q_OS_LINUX) && ! defined(Q_OS_WINDOWS)
#error "unsupported platform"
#endif
    const QString &tmpFile = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QLockFile file(tmpFile + "/61fd13c5-539c-4db3-8acd-139f0e9a6beb");
    if (! file.tryLock()) {
        return 2;
    }

    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);
    signal(SIGTERM, signal_handler);

    QApplication a(argc, argv);
#ifdef Q_OS_LINUX
    if (QApplication::platformName() != "xcb") {
        qCritical() << "不支持xcb";
        return 1;
    }
#endif

    QApplication::setQuitOnLastWindowClosed(false);
    MainWindow w;
    return a.exec();
}
