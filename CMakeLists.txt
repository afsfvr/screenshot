cmake_minimum_required(VERSION 3.16)
project(screenshot LANGUAGES CXX VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message(STATUS "CMake build type: ${CMAKE_BUILD_TYPE}")

option(TENCENT_OCR "use tencent ocr" ON)
option(RAPID_OCR "use rapid ocr" OFF)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Gui Widgets)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# get_cmake_property(_variableNames VARIABLES)
# foreach(_variableName ${_variableNames})
#     if(_variableName MATCHES "^(QT|Qt)")
#         message(STATUS "${_variableName} = ${${_variableName}}")
#     endif()
# endforeach()

add_executable(${PROJECT_NAME}
    src/BaseWindow.cpp
    src/GifWidget.cpp
    src/MySliderStyle.cpp
    src/SettingWidget.cpp
    src/Shape.cpp
    src/Tool.cpp
    src/TopWidget.cpp
    src/gif.cpp
    src/main.cpp
    src/mainwindow.cpp
    src/qaesencryption.cpp
    src/BaseWindow.h
    src/BlockQueue.h
    src/GifWidget.h
    src/MySliderStyle.h
    src/SettingWidget.h
    src/Shape.h
    src/Tool.h
    src/TopWidget.h
    src/gif.h
    src/mainwindow.h
    src/qaesencryption.h
    src/SettingWidget.ui
    src/Tool.ui
    res.qrc
)

if(TENCENT_OCR)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Network)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt::Network)
    target_sources(${PROJECT_NAME} PRIVATE src/OcrImpl/TencentOcr.cpp)
    target_compile_definitions(${PROJECT_NAME} PRIVATE TENCENT_OCR OCR)
    set(OCR ON CACHE BOOL "Enable OCR support")
    set(OCR_STATUS "TENCENT")
elseif(RAPID_OCR)
    set(MODELS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/rapidocr/models")
    set(MODELS_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    file(COPY ${MODELS_SOURCE_DIR} DESTINATION ${MODELS_OUTPUT_DIR})
    target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/rapidocr/include)
    target_link_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/rapidocr/lib)
    target_sources(${PROJECT_NAME} PRIVATE src/OcrImpl/RapidOcr.cpp)
    target_compile_definitions(${PROJECT_NAME} PRIVATE RAPID_OCR OCR)
    set(OCR ON CACHE BOOL "Enable OCR support")
    set(OCR_STATUS "RAPID")
else()
    set(OCR OFF CACHE BOOL "Enable OCR support")
    set(OCR_STATUS "NONE")
endif()
message(STATUS "OCR backend: ${OCR_STATUS}")

if(OCR)
    target_sources(${PROJECT_NAME} PRIVATE src/Ocr.cpp)
endif()

if(UNIX)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS X11Extras)
    target_sources(${PROJECT_NAME} PRIVATE src/KeyMouseEvent.cpp src/KeyMouseEvent.h)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt::X11Extras X11 Xext Xtst)

    find_package(OpenCV QUIET COMPONENTS core imgproc imgcodecs)
    if(NOT OpenCV_FOUND)
        message(WARNING "OpenCV not found. Long screenshot feature will be disabled.")
    else()
        target_sources(${PROJECT_NAME} PRIVATE src/LongWidget.cpp src/LongWidget.h)
        target_compile_definitions(${PROJECT_NAME} PRIVATE LONG_SCREENSHOT)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${OpenCV_LIBS})
    endif()
elseif(WIN32)
    find_package(OpenCV QUIET COMPONENTS core imgproc imgcodecs)
    target_sources(${PROJECT_NAME} PRIVATE src/LongWidget.cpp src/LongWidget.h)
    target_compile_definitions(${PROJECT_NAME} PRIVATE LONG_SCREENSHOT)
    if(OpenCV_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${OpenCV_LIBS})
    else()
        target_include_directories(${PROJECT_NAME} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/opencv/include/opencv4
        )
        target_link_directories(${PROJECT_NAME} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/opencv/lib/windows/mingw
        )
        target_link_libraries(${PROJECT_NAME} PRIVATE 
            opencv_imgproc480
            opencv_imgcodecs480
            opencv_core480
            z
        )
    endif()
    target_sources(${PROJECT_NAME} PRIVATE resource.rc)
    target_link_libraries(${PROJECT_NAME} PRIVATE Dwmapi user32)
endif()
message(STATUS "OpenCV Found: ${OpenCV_FOUND}, Version: ${OpenCV_VERSION}")

target_compile_options(${PROJECT_NAME} PRIVATE -Wall)
target_link_libraries(${PROJECT_NAME} PRIVATE Qt::Core Qt::Gui Qt::Widgets)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
if(DEFINED ENV{GITHUB_VERSION})
    set(CPACK_PACKAGE_VERSION "$ENV{GITHUB_VERSION}")
else()
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
endif()
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "一个截图工具")
set(CPACK_PACKAGE_DESCRIPTION "使用Qt开发的截图工具，支持多屏幕、区域和窗口截图。\n提供简单编辑和 GIF 录制功能。")
set(CPACK_PACKAGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/package/linux/screenshot.png")
set(CPACK_PACKAGE_CHECKSUM "SHA256")
if(UNIX)
    install(TARGETS ${PROJECT_NAME} DESTINATION bin)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/package/linux/screenshot.desktop DESTINATION share/applications)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/package/linux/screenshot.png DESTINATION share/icons/hicolor/32x32/apps)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_FILE_NAME "${PROJECT_NAME}.deb")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/afsfvr/screenshot")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "afsfvr <2461519090@qq.com>")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
        "${CMAKE_CURRENT_SOURCE_DIR}/package/linux/prerm;${CMAKE_CURRENT_SOURCE_DIR}/package/linux/postrm")
elseif(WIN32)
    install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/package/linux/screenshot.png")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/package/linux/screenshot.png")
    set(CPACK_NSIS_DISPLAY_NAME "${PROJECT_NAME}")
    set(CPACK_NSIS_CREATE_ICONS_EXTRA "CreateShortcut '\\$SMPROGRAMS\\$STARTMENU_FOLDER\\${PROJECT_NAME}.lnk' '\\$INSTDIR\\bin\\${PROJECT_NAME}.exe'")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA "Delete '\\$SMPROGRAMS\\${PROJECT_NAME}.lnk'")
    set(CPACK_NSIS_CREATE_DESKTOP_ICON "CreateShortcut '$DESKTOP\\${PROJECT_NAME}.lnk' '$INSTDIR\\bin\\${PROJECT_NAME}.exe'")
endif()
include(CPack)
