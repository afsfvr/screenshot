# 复制到rapid安装目录
set(RapidOcr_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")
find_library(
    RapidOcr_LIBRARY
    NAMES RapidOcrOnnx
    PATHS "${CMAKE_CURRENT_LIST_DIR}/lib"
    NO_DEFAULT_PATH
)
find_library(
    ONNXRUNTIME_LIBRARY
    NAMES onnxruntime
    PATHS "${CMAKE_CURRENT_LIST_DIR}/../../onnxruntime-static/linux/lib" # 修改为onnxruntime路径
    NO_DEFAULT_PATH
)
add_library(RapidOcrOnnx SHARED IMPORTED)
set_target_properties(RapidOcrOnnx PROPERTIES
    IMPORTED_LOCATION "${RapidOcr_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${RapidOcr_INCLUDE_DIRS}"
    INTERFACE_LINK_LIBRARIES "${ONNXRUNTIME_LIBRARY}"
) 
set(RapidOcr_LIBS RapidOcrOnnx)
