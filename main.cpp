#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    if (QApplication::platformName() != "xcb") {
        qCritical() << "不支持xcb";
        return 1;
    }
    QApplication a(argc, argv);

    QApplication::setQuitOnLastWindowClosed(false);
    MainWindow w;
    return a.exec();
}
