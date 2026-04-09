# 添加Google Test（可选）
option(USE_GOOGLETEST "Build tests with Google Test" OFF)

if(USE_GOOGLETEST)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.1
    )
    
    FetchContent_MakeAvailable(googletest)
    
    # 添加测试
    add_executable(matrix_test
        test_matrix.cpp
    )
    
    target_link_libraries(matrix_test
        PRIVATE matrix_lib GTest::gtest GTest::gtest_main
    )
    
    add_test(
        NAME matrix_tests
        COMMAND matrix_test
    )
endif()

# 基础测试
add_executable(simple_test
    test_core.cpp
)

target_link_libraries(simple_test
    PRIVATE matrix_lib
)

add_test(
    NAME simple_matrix_test
    COMMAND simple_test
)

# Python测试
add_test(
    NAME python_import_test
    COMMAND ${Python3_EXECUTABLE} -c "import pymatrix; print('PyMatrix imported successfully')"
)

# 性能测试
# add_test(
#     NAME performance_test
#     COMMAND benchmark --size 128
# )