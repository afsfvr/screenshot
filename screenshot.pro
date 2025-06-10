QT       += core gui

unix: {
    QT += x11extras
    LIBS += -lX11 -lXext -lXtst
    SOURCES += src/KeyMouseEvent.cpp
    HEADERS += src/KeyMouseEvent.h
}
win32: {
    LIBS += -lDwmapi  -luser32
}
win32-msvc*: {
    QMAKE_CFLAGS *= /utf-8
    QMAKE_CXXFLAGS *= /utf-8
}
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    src/BaseWindow.cpp \
    src/GifWidget.cpp \
    src/SettingWidget.cpp \
    src/Shape.cpp \
    src/Tool.cpp \
    src/TopWidget.cpp \
    src/gif.cpp \
    src/main.cpp \
    src/mainwindow.cpp

HEADERS += \
    src/BaseWindow.h \
    src/GifWidget.h \
    src/SettingWidget.h \
    src/Shape.h \
    src/Tool.h \
    src/TopWidget.h \
    src/gif.h \
    src/mainwindow.h

# DESTDIR = build/Desktop_Qt_5_15_2_GCC_64bit-Debug
# UI_DIR  = build/Desktop_Qt_5_15_2_GCC_64bit-Debug
# MOC_DIR = build/Desktop_Qt_5_15_2_GCC_64bit-Debug
# RCC_DIR = build/Desktop_Qt_5_15_2_GCC_64bit-Debug
# OBJECTS_DIR = build/Desktop_Qt_5_15_2_GCC_64bit-Debug

FORMS += \
    src/SettingWidget.ui \
    src/Tool.ui

RESOURCES += \
    res.qrc


OTHER_FILES += resource.rc
RC_FILE = resource.rc

# RC_ICONS = ./images/screenshot.ico
# VERSION=1.0
# QMAKE_TARGET_PRODUCT=截图
# QMAKE_TARGET_COMPANY=公司名称
# QMAKE_TARGET_DESCRIPTION=描述
# QMAKE_TARGET_COPYRIGHT=版权
# RC_LANG=0x0804
