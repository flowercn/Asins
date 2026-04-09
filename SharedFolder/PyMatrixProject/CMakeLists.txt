cmake_minimum_required(VERSION 3.15)
project(PyMatrix VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 查找OpenMP
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    message(STATUS "Found OpenMP: ${OpenMP_CXX_VERSION}")
endif()

# 跨平台编译选项
if(MSVC)
    # Windows/MSVC
    add_compile_options(/W4 /WX /O2 /fp:fast /EHsc)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DNOMINMAX)  # 避免min/max宏冲突
elseif(APPLE)
    # macOS/Clang
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -O3 -ffast-math -march=native)
    add_compile_options(-fcolor-diagnostics)
else()
    # Linux/GCC
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -O3 -ffast-math -march=native)
    add_compile_options(-Wno-psabi)  # 避免ARM上的冗长警告
    
    # SIMD检测
    option(USE_SIMD "Enable SIMD optimizations" ON)
    if(USE_SIMD)
        # 检测CPU支持的指令集
        include(CheckCXXCompilerFlag)
        set(SIMD_FLAGS)
        
        # AVX512 (最高优先级)
        check_cxx_compiler_flag(-mavx512f HAS_AVX512)
        if(HAS_AVX512)
            list(APPEND SIMD_FLAGS -mavx512f -mavx512cd -mavx512vl -mavx512bw -mavx512dq)
            add_definitions(-D__AVX512__)
        endif()
        
        # AVX2
        check_cxx_compiler_flag(-mavx2 HAS_AVX2)
        if(HAS_AVX2 AND NOT HAS_AVX512)
            list(APPEND SIMD_FLAGS -mavx2 -mfma)
            add_definitions(-D__AVX2__)
        endif()
        
        # SSE4.2 (基本保证)
        check_cxx_compiler_flag(-msse4.2 HAS_SSE42)
        if(HAS_SSE42)
            list(APPEND SIMD_FLAGS -msse4.2)
            add_definitions(-D__SSE4_2__)
        endif()
        
        if(SIMD_FLAGS)
            add_compile_options(${SIMD_FLAGS})
            message(STATUS "Enabled SIMD flags: ${SIMD_FLAGS}")
        endif()
    endif()
endif()

# 设置输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 根据配置类型设置不同的编译选项
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

string(TOUPPER ${CMAKE_BUILD_TYPE} BUILD_TYPE_UPPER)
if(BUILD_TYPE_UPPER STREQUAL "DEBUG")
    add_compile_options(-g -O0 -DDEBUG)
else()
    add_compile_options(-DNDEBUG)
endif()

# 查找Python和NumPy
find_package(Python3 REQUIRED COMPONENTS Interpreter Development NumPy)
message(STATUS "Found Python: ${Python3_EXECUTABLE}")
message(STATUS "Python includes: ${Python3_INCLUDE_DIRS}")
message(STATUS "Python libraries: ${Python3_LIBRARIES}")
message(STATUS "NumPy includes: ${Python3_NumPy_INCLUDE_DIRS}")

# 添加pybind11（支持自动下载）
include(FetchContent)
FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG v2.11.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(pybind11)

# 检查pybind11是否找到Python
if(NOT pybind11_FOUND)
    message(FATAL_ERROR "pybind11 not found!")
endif()

# 添加子目录
add_subdirectory(src)

# 配置测试
enable_testing()
# if(BUILD_TESTING)
    add_subdirectory(tests)
# endif()

# 安装配置
install(
    DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp"
)

install(
    TARGETS matrix_lib pymatrix
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# 生成配置头文件
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/config.hpp
)

# 创建编译后复制Python模块的脚本
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts/install_module.py.in
    ${CMAKE_BINARY_DIR}/install_module.py
    @ONLY
)

# 在构建后自动复制Python模块
# (Moved to src/CMakeLists.txt because target must be in same directory)