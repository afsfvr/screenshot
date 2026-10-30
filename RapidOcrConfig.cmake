# 这个文件复制到rapid安装目录
set(RapidOcr_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")

include("${CMAKE_CURRENT_LIST_DIR}/../../onnxruntime-static/linux/OnnxRuntimeConfig.cmake") # 修改为onnxruntime路径

find_library(
    RapidOcr_LIBRARY
    NAMES RapidOcrOnnx
    PATHS "${CMAKE_CURRENT_LIST_DIR}/lib"
    NO_DEFAULT_PATH
)

add_library(RapidOcrOnnx SHARED IMPORTED)
set_target_properties(RapidOcrOnnx PROPERTIES
    IMPORTED_LOCATION "${RapidOcr_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${RapidOcr_INCLUDE_DIRS};${OnnxRuntime_INCLUDE_DIRS}"
    # INTERFACE_LINK_LIBRARIES "${OnnxRuntime_LIBS}"
)
set(RapidOcr_LIBS RapidOcrOnnx)
