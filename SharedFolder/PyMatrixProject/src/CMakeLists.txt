# 主矩阵库（静态库）
add_library(matrix_lib STATIC
    Matrix.cpp
)


# 设置包含目录
target_include_directories(matrix_lib
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/../include
)

# 链接数学库（如果使用数学函数）
if(UNIX AND NOT APPLE)
    target_link_libraries(matrix_lib PUBLIC m)  # 链接libm
endif()

# 链接OpenMP
if(OpenMP_CXX_FOUND)
    target_link_libraries(matrix_lib PUBLIC OpenMP::OpenMP_CXX)
endif()

# 设置目标属性
set_target_properties(matrix_lib PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    POSITION_INDEPENDENT_CODE ON  # 允许用于共享库
)

# Python扩展模块
pybind11_add_module(pymatrix
    bindings.cpp
)

# 在构建后自动复制Python模块
add_custom_command(
    TARGET pymatrix POST_BUILD
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_BINARY_DIR}/install_module.py
    COMMENT "Installing Python module..."
)

# 链接矩阵库
target_link_libraries(pymatrix
    PRIVATE matrix_lib
)

# 设置模块属性
set_target_properties(pymatrix PROPERTIES
    OUTPUT_NAME "pymatrix"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# 在macOS上设置安装名称
if(APPLE)
    set_target_properties(pymatrix PROPERTIES
        INSTALL_RPATH "@loader_path"
        BUILD_WITH_INSTALL_RPATH ON
    )
endif()

# 测试程序
# add_executable(cpp_test
#     test.cpp
# )

# target_link_libraries(cpp_test
#     PRIVATE matrix_lib
# )

# 性能测试程序
# add_executable(benchmark
#     benchmark.cpp
# )

# target_link_libraries(benchmark
#     PRIVATE matrix_lib
# )

# if(UNIX AND NOT APPLE)
#     target_link_libraries(benchmark m)
# endif()