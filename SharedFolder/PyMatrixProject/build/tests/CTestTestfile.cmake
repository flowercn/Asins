# CMake generated Testfile for 
# Source directory: /home/v/桌面/PyMatrixProject/tests
# Build directory: /home/v/桌面/PyMatrixProject/build/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(simple_matrix_test "/home/v/桌面/PyMatrixProject/build/bin/simple_test")
set_tests_properties(simple_matrix_test PROPERTIES  _BACKTRACE_TRIPLES "/home/v/桌面/PyMatrixProject/tests/CMakeLists.txt;38;add_test;/home/v/桌面/PyMatrixProject/tests/CMakeLists.txt;0;")
add_test(python_import_test "/usr/bin/python3" "-c" "import pymatrix; print('PyMatrix imported successfully')")
set_tests_properties(python_import_test PROPERTIES  _BACKTRACE_TRIPLES "/home/v/桌面/PyMatrixProject/tests/CMakeLists.txt;44;add_test;/home/v/桌面/PyMatrixProject/tests/CMakeLists.txt;0;")
