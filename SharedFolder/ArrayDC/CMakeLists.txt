cmake_minimum_required(VERSION 3.16)

project(Array VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. 查找依赖包
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools Charts SerialPort OpenGLWidgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools Charts SerialPort OpenGLWidgets)
# 查找系统 OpenGL
find_package(OpenGL REQUIRED)

set(TS_FILES Array_en_US.ts)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        ${TS_FILES}
        protocol_parser.cpp
        protocol_parser.h
        async_logger.h
        async_logger.cpp
        data_processor.h
        data_processor.cpp
        replay_thread.h
        replay_thread.cpp
        ins_solver.h      # 🌟 新增
        ins_solver.cpp    # 🌟 新增
        # flight_view.h   <-- 删除
        # flight_view.cpp <-- 删除
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(Array MANUAL_FINALIZATION ${PROJECT_SOURCES}
        replay_thread.h
        replay_thread.cpp
        ins_solver.h
        ins_solver.cpp)
    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    if(ANDROID)
        add_library(Array SHARED ${PROJECT_SOURCES})
    else()
        add_executable(Array ${PROJECT_SOURCES})
    endif()
    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

# 🌟 修复关键：将当前源码目录加入包含路径
target_include_directories(Array PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 2. 链接库
if(WIN32)
    target_link_libraries(Array PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt6::Charts
        Qt6::SerialPort
        Qt6::OpenGLWidgets
        glu32
    )
else()
    target_link_libraries(Array PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt6::Charts
        Qt6::SerialPort
        Qt6::OpenGLWidgets
        OpenGL::GL
    )
endif()

# MacOS 设置
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.Array)
endif()
set_target_properties(Array PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS Array
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(Array)
endif()
