#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
#if ! defined(Q_OS_LINUX) && ! defined(Q_OS_WINDOWS)
#error "unsupported platform"
#endif

#ifdef Q_OS_LINUX
    if (QApplication::platformName() != "xcb") {
        qCritical() << "不支持xcb";
        return 1;
    }
#endif
    QApplication a(argc, argv);

    QApplication::setQuitOnLastWindowClosed(false);
    MainWindow w;
    return a.exec();
}
