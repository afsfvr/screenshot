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

option(TENCENT_OCR "use tencent ocr" OFF)
option(RAPID_OCR "use rapid ocr" OFF)
option(PADDLE_OCR "use paddle ocr" OFF)
option(STATIC_LINK "Link all dependencies statically" OFF)
option(ENABLE_ZXING "Enable ZXing QRCode reader" ON)
# OCR 后端可任意组合启用，多个后端编译后在设置中切换

SET(PADDLE_LIB "" CACHE PATH "paddle lib")

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

# OCR 后端（相互独立，可任意组合同时启用）
set(OCR OFF)
if(TENCENT_OCR)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Network)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt::Network)
    target_sources(${PROJECT_NAME} PRIVATE src/OcrImpl/TencentOcrImpl.cpp)
    target_compile_definitions(${PROJECT_NAME} PRIVATE TENCENT_OCR OCR)
    set(OCR ON)
    message(STATUS "OCR backend: TencentOcr")
endif()

if(RAPID_OCR)
    find_package(RapidOcr REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${RapidOcr_LIBS})
    target_sources(${PROJECT_NAME} PRIVATE src/OcrImpl/RapidOcrImpl.cpp)
    target_compile_definitions(${PROJECT_NAME} PRIVATE RAPID_OCR OCR)
    set(OCR ON)
    message(STATUS "OCR backend: RapidOcr")

    set_property(TARGET ${PROJECT_NAME} PROPERTY INSTALL_RPATH "$ORIGIN/../lib/${PROJECT_NAME}")
    install(FILES ${RapidOcr_LIBRARY} DESTINATION lib/${PROJECT_NAME})
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/third_party/rapidocr/models/ DESTINATION share/${PROJECT_NAME}/models)
    get_filename_component(RapidOcr_LIB_DIR "${RapidOcr_LIBRARY}" DIRECTORY)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rapidocr/models"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/models"
    )
endif()

if(PADDLE_OCR)
    if(NOT DEFINED PADDLE_LIB)
        message(FATAL_ERROR "please set PADDLE_LIB with -DPADDLE_LIB=/path/paddle/lib")
    endif()
    if(NOT EXISTS "${PADDLE_LIB}")
        message(FATAL_ERROR "PADDLE_LIB does not exist: ${PADDLE_LIB}")
    endif()

    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/third_party/PaddleOCR/deploy/cpp_infer")
    target_link_directories(${PROJECT_NAME} PRIVATE "${PADDLE_LIB}/third_party/install/glog/lib")
    target_link_directories(${PROJECT_NAME} PRIVATE "${PADDLE_LIB}/third_party/install/gflags/lib")
    target_link_directories(${PROJECT_NAME} PRIVATE "${PADDLE_LIB}/third_party/install/xxhash/lib")
    target_link_directories(${PROJECT_NAME} PRIVATE "${PADDLE_LIB}/third_party/install/yaml-cpp/lib")
    target_link_directories(${PROJECT_NAME} PRIVATE "${PADDLE_LIB}/third_party/install/protobuf/lib")
    target_link_directories(${PROJECT_NAME} PRIVATE "${PADDLE_LIB}/third_party/install/cryptopp/lib")
    target_link_directories(${PROJECT_NAME} PRIVATE "${PADDLE_LIB}/third_party/install/utf8proc/lib")
    target_link_directories(${PROJECT_NAME} PRIVATE "${PADDLE_LIB}/third_party/install/tbb/lib")
    target_link_directories(${PROJECT_NAME} PRIVATE "${PADDLE_LIB}/third_party/install/openvino/intel64")
    target_link_directories(${PROJECT_NAME} PRIVATE "${PADDLE_LIB}/third_party/install/onednn/lib")
    target_link_directories(${PROJECT_NAME} PRIVATE "${PADDLE_LIB}/third_party/install/mklml/lib")
    target_link_directories(${PROJECT_NAME} PRIVATE "${PADDLE_LIB}/paddle/lib")
    target_link_libraries(${PROJECT_NAME} PRIVATE ppocr)
    target_sources(${PROJECT_NAME} PRIVATE src/OcrImpl/PaddleOcrImpl.cpp)
    target_compile_definitions(${PROJECT_NAME} PRIVATE PADDLE_OCR OCR)
    set(OCR ON)
    message(STATUS "OCR backend: PaddleOcr")

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/PaddleOCR/deploy/cpp_infer/src/configs"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/configs"
    )
endif()

if(NOT OCR)
    message(STATUS "OCR backend: NONE")
endif()

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
    set(ZXING_WRITERS ON CACHE STRING "" FORCE)
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
    install(CODE "
        file(REMOVE_RECURSE
            \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/include/\"
            \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/lib/x86_64-linux-gnu/\"
        )
    ")
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
target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CXX_COMPILER_ID:MSVC>:NOMINMAX>)
target_link_libraries(${PROJECT_NAME} PRIVATE Qt::Core Qt::Gui Qt::Widgets)

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
if(DEFINED ENV{GITHUB_VERSION})
    set(CPACK_PACKAGE_VERSION "$ENV{GITHUB_VERSION}")
else()
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
endif()

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "一个截图工具")
set(CPACK_PACKAGE_DESCRIPTION "使用Qt开发的截图工具，支持多屏幕、区域和窗口截图。\n提供简单编辑和 GIF 录制功能。")
set(CPACK_PACKAGE_CHECKSUM "SHA256")
set(CPACK_PACKAGE_VENDOR "afsfvr")
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
if(UNIX AND NOT APPLE)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/package/linux/screenshot.desktop DESTINATION share/applications)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/package/linux/screenshot.png DESTINATION share/icons/hicolor/32x32/apps)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_FILE_NAME "${PROJECT_NAME}.deb")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    if(RAPID_OCR)
        set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS "${RapidOcr_LIB_DIR}")
    endif()
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/afsfvr/screenshot")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "afsfvr <2461519090@qq.com>")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
        "${CMAKE_CURRENT_SOURCE_DIR}/package/linux/prerm"
        "${CMAKE_CURRENT_SOURCE_DIR}/package/linux/postrm"
    )
elseif(WIN32)
    # 定位 windeployqt
    set(_qt_bin_dirs "")
    foreach(_hint "${Qt${QT_VERSION_MAJOR}_DIR}/../../../bin" "${Qt${QT_VERSION_MAJOR}_DIR}/../../bin")
        get_filename_component(_qt_bin_abs "${_hint}" ABSOLUTE)
        list(APPEND _qt_bin_dirs "${_qt_bin_abs}")
    endforeach()
    find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt HINTS ${_qt_bin_dirs} NO_DEFAULT_PATH)
    if(NOT WINDEPLOYQT_EXECUTABLE)
        find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt)
    endif()
    if(NOT WINDEPLOYQT_EXECUTABLE)
        message(FATAL_ERROR "找不到 windeployqt.exe，无法在打包时捆绑 Qt 运行时 DLL")
    endif()
    message(STATUS "windeployqt: ${WINDEPLOYQT_EXECUTABLE}")
    file(TO_CMAKE_PATH "${WINDEPLOYQT_EXECUTABLE}" _windeployqt_path)

    # 安装时用 windeployqt 把 Qt 依赖 DLL 部署到 bin 目录
    install(CODE "
        if(CMAKE_INSTALL_CONFIG_NAME STREQUAL \"Debug\")
            set(deploy_config --debug)
        else()
            set(deploy_config --release)
        endif()
        execute_process(COMMAND
            \"${_windeployqt_path}\"
            \${deploy_config}
            \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/bin/${PROJECT_NAME}.exe\"
            RESULT_VARIABLE deploy_result
        )

        if(NOT deploy_result EQUAL 0)
            message(FATAL_ERROR \"windeployqt 部署失败\")
        endif()
    ")

    set(CPACK_GENERATOR "NSIS")
    set(CPACK_PACKAGE_ICON "${PROJECT_SOURCE_DIR}/images/screenshot.ico")
    set(CPACK_NSIS_MUI_ICON "${PROJECT_SOURCE_DIR}/images/screenshot.ico")
    set(CPACK_NSIS_MUI_UNIICON "${PROJECT_SOURCE_DIR}/images/screenshot.ico")
    set(CPACK_NSIS_PACKAGE_NAME "${PROJECT_NAME}")
    set(CPACK_NSIS_DISPLAY_NAME "${PROJECT_NAME}")
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_NAME}")
    set(CPACK_NSIS_UNINSTALL_NAME "Uninstall")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\${PROJECT_NAME}.exe")
    set(CPACK_NSIS_CREATE_ICONS_EXTRA "CreateShortcut '$SMPROGRAMS\\$STARTMENU_FOLDER\\${PROJECT_NAME}.lnk' '$INSTDIR\\bin\\${PROJECT_NAME}.exe'")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA "Delete '$SMPROGRAMS\\$STARTMENU_FOLDER\\${PROJECT_NAME}.lnk'")
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "CreateShortcut '$DESKTOP\\${PROJECT_NAME}.lnk' '$INSTDIR\\bin\\${PROJECT_NAME}.exe'")
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "Delete '$DESKTOP\\${PROJECT_NAME}.lnk'")
endif()
include(CPack)
