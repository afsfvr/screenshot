cmake_minimum_required(VERSION 3.16)
project(screenshot LANGUAGES CXX VERSION 1.3.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()
message(STATUS "Compiler ID: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "CMake generator: ${CMAKE_GENERATOR}")
message(STATUS "CMake build type: ${CMAKE_BUILD_TYPE}")

option(TENCENT_OCR "use tencent ocr" ON)
option(RAPID_OCR "use rapid ocr" OFF)
option(STATIC_LINK "Link all dependencies statically" OFF)
option(ENABLE_ZXING "Enable ZXing QRCode reader" ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

message(STATUS "QT_VERSION: ${QT_VERSION}")
message(STATUS "Qt library path: ${Qt${QT_VERSION_MAJOR}_DIR}")
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
    src/main.cpp
    src/mainwindow.cpp
    src/BaseWindow.h
    src/BlockQueue.h
    src/GifWidget.h
    src/MySliderStyle.h
    src/SettingWidget.h
    src/Shape.h
    src/Tool.h
    src/TopWidget.h
    src/mainwindow.h
    src/ui/SettingWidget.ui
    src/ui/Tool.ui
    res.qrc
)

set_property(TARGET ${PROJECT_NAME} PROPERTY AUTOUIC_SEARCH_PATHS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
)

if (STATIC_LINK)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
endif(STATIC_LINK)

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

# GIF
add_library(giflib STATIC
    third_party/gif/gif.cpp
    third_party/gif/gif.h
)
target_include_directories(giflib PUBLIC third_party/gif)
target_link_libraries(${PROJECT_NAME} PRIVATE giflib)

# QAES encryption
if (TENCENT_OCR)
    add_library(qaesencryption STATIC
        third_party/qaesencryption/qaesencryption.cpp
        third_party/qaesencryption/qaesencryption.h
    )
    target_include_directories(qaesencryption PUBLIC third_party/qaesencryption)
    target_link_libraries(qaesencryption PUBLIC Qt::Core)
    target_link_libraries(${PROJECT_NAME} PRIVATE qaesencryption)
endif(TENCENT_OCR)

# opencv
find_package(OpenCV QUIET COMPONENTS core imgproc imgcodecs)
if(NOT OpenCV_FOUND)
    message(STATUS "OpenCV not found. Long screenshot feature will be disabled.")
else()
    target_sources(${PROJECT_NAME} PRIVATE src/LongWidget.cpp src/LongWidget.h)
    target_compile_definitions(${PROJECT_NAME} PRIVATE LONG_SCREENSHOT)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${OpenCV_LIBS})
    message(STATUS "OpenCV Version: ${OpenCV_VERSION}")
endif()

if(ENABLE_ZXING)
    # ZXing 配置
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(ZXING_READERS ON CACHE BOOL "" FORCE)
    set(ZXING_WRITERS OFF CACHE STRING "" FORCE)
    set(ZXING_C_API OFF CACHE BOOL "" FORCE)
    set(ZXING_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(ZXING_EXAMPLES_QT OFF CACHE BOOL "" FORCE)
    set(ZXING_UNIT_TESTS OFF CACHE BOOL "" FORCE)
    set(ZXING_BLACKBOX_TESTS OFF CACHE BOOL "" FORCE)

    include(FetchContent)
    FetchContent_Declare(
        ZXing
        URL https://github.com/zxing-cpp/zxing-cpp/releases/download/v3.1.0/zxing-cpp-3.1.0.zip
        URL_HASH SHA256=2f879b1758dc5aa0bd8bc33b9f5db1d84340548a65c9869c7c8c74f7b95411ff
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(ZXing)
    target_link_libraries(ZXing PUBLIC Qt::Core Qt::Gui)
    target_link_libraries(${PROJECT_NAME} PRIVATE ZXing::ZXing)
    target_compile_definitions(${PROJECT_NAME} PRIVATE QRCODE)
endif()

if(UNIX)
    target_sources(${PROJECT_NAME} PRIVATE src/KeyMouseEvent.cpp src/KeyMouseEvent.h)
    if(QT_VERSION_MAJOR LESS 6)
        find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS X11Extras)
        target_link_libraries(${PROJECT_NAME} PRIVATE Qt::X11Extras)
    endif()
    target_link_libraries(${PROJECT_NAME} PRIVATE X11 Xext Xtst xcb)
elseif(WIN32)
    target_sources(${PROJECT_NAME} PRIVATE resource.rc)
    target_link_libraries(${PROJECT_NAME} PRIVATE Dwmapi user32)
endif()

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8 /W4>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>

    $<$<AND:$<CXX_COMPILER_ID:GNU>,$<CONFIG:Debug>>:-g -ggdb -O0>
    $<$<AND:$<CXX_COMPILER_ID:Clang>,$<CONFIG:Debug>>:-g -O0>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/Zi /Od>

    $<$<AND:$<CXX_COMPILER_ID:GNU>,$<CONFIG:Release>>:-O2>
    $<$<AND:$<CXX_COMPILER_ID:Clang>,$<CONFIG:Release>>:-O2>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/O2>

    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
        -fmacro-prefix-map=${PROJECT_SOURCE_DIR}/=
        -ffile-prefix-map=${PROJECT_SOURCE_DIR}/=
    >
)
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
