QT       += core gui

unix: {
    QT += x11extras
    LIBS += -lX11 -lXext -lXtst
    SOURCES += KeyMouseEvent.cpp
    HEADERS += KeyMouseEvent.h
    aaa
}

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    BaseWindow.cpp \
    Shape.cpp \
    Tool.cpp \
    TopWidget.cpp \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    BaseWindow.h \
    Shape.h \
    Tool.h \
    TopWidget.h \
    mainwindow.h

# DESTDIR = build/Desktop_Qt_5_15_2_GCC_64bit-Debug
# UI_DIR  = build/Desktop_Qt_5_15_2_GCC_64bit-Debug
# MOC_DIR = build/Desktop_Qt_5_15_2_GCC_64bit-Debug
# RCC_DIR = build/Desktop_Qt_5_15_2_GCC_64bit-Debug
# OBJECTS_DIR = build/Desktop_Qt_5_15_2_GCC_64bit-Debug

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

FORMS += \
    Tool.ui

RESOURCES += \
    res.qrc
