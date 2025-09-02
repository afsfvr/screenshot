QT       += core gui widgets

CONFIG += c++17

DEFINES += TENCENT_OCR

unix: {
    QT += x11extras
    LIBS += -lX11 -lXext -lXtst
    LIBS += -L$$PWD/opencv/lib/linux -lopencv_imgproc -lopencv_imgcodecs -lopencv_core
    SOURCES += src/KeyMouseEvent.cpp
    HEADERS += src/KeyMouseEvent.h
}
win32: {
    LIBS += -lDwmapi  -luser32
    LIBS += -L$$PWD/opencv/lib/windows/mingw -lopencv_imgproc480 -lopencv_imgcodecs480 -lopencv_core480 -lz
}
win32-msvc*: {
    QMAKE_CFLAGS *= /utf-8
    QMAKE_CXXFLAGS *= /utf-8
}

INCLUDEPATH += $$PWD/opencv/include/opencv4

contains(DEFINES, RAPID_OCR) {
    models.path=$$OUT_PWD
    models.files=$$PWD/rapidocr/models
    COPIES += models
    INCLUDEPATH += $$PWD/rapidocr/include
    LIBS += -L$$PWD/rapidocr/lib -lRapidOcrOnnx
    SOURCES += src/OcrImpl/RapidOcr.cpp
    HEADERS += src/OcrImpl/RapidOcr.h
    ! contains(DEFINES, OCR) {
        DEFINES += OCR
    }
}
contains(DEFINES, TENCENT_OCR) {
    QT += network
    SOURCES += src/OcrImpl/TencentOcr.cpp
    HEADERS += src/OcrImpl/TencentOcr.h
    ! contains(DEFINES, OCR) {
        DEFINES += OCR
    }
}
contains(DEFINES, OCR) {
    SOURCES += src/Ocr.cpp
    HEADERS += src/Ocr.h
}


# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    src/BaseWindow.cpp \
    src/GifWidget.cpp \
    src/LongWidget.cpp \
    src/SettingWidget.cpp \
    src/Shape.cpp \
    src/Tool.cpp \
    src/TopWidget.cpp \
    src/gif.cpp \
    src/main.cpp \
    src/mainwindow.cpp

HEADERS += \
    src/BaseWindow.h \
    src/BlockQueue.h \
    src/GifWidget.h \
    src/LongWidget.h \
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


OTHER_FILES += resource.rc README.md
RC_FILE = resource.rc

# RC_ICONS = ./images/screenshot.ico
# VERSION=1.0
# QMAKE_TARGET_PRODUCT=截图
# QMAKE_TARGET_COMPANY=公司名称
# QMAKE_TARGET_DESCRIPTION=描述
# QMAKE_TARGET_COPYRIGHT=版权
# RC_LANG=0x0804
