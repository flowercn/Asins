#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include <chrono>
#include "pymatrix/Matrix.hpp"

namespace py = pybind11;

// 将numpy数组转换为Matrix
template<typename T>
Matrix<T> from_numpy(py::array_t<T> arr) {
    auto buf = arr.request();
    if (buf.ndim != 2) {
        throw std::runtime_error("Number of dimensions must be two");
    }
    
    size_t rows = buf.shape[0];
    size_t cols = buf.shape[1];
    
    Matrix<T> mat(rows, cols);
    T* ptr = static_cast<T*>(buf.ptr);
    
    // 复制数据（考虑numpy可能是行主序或列主序）
    for (size_t i = 0; i < rows; ++i) {
        for (size_t j = 0; j < cols; ++j) {
            mat(i, j) = ptr[i * cols + j];
        }
    }
    
    return mat;
}

// 将Matrix转换为numpy数组
template<typename T>
py::array_t<T> to_numpy(const Matrix<T>& mat) {
    auto result = py::array_t<T>({mat.rows(), mat.cols()});
    auto buf = result.request();
    T* ptr = static_cast<T*>(buf.ptr);
    
    for (size_t i = 0; i < mat.rows(); ++i) {
        for (size_t j = 0; j < mat.cols(); ++j) {
            ptr[i * mat.cols() + j] = mat(i, j);
        }
    }
    
    return result;
}

// Python模块定义
PYBIND11_MODULE(pymatrix, m) {
    m.doc() = "High-performance matrix library written in C++";
    
    // 绑定Matrix<float>
    py::class_<Matrix<float>>(m, "MatrixFloat")
        .def(py::init<size_t, size_t>())
        .def_static("from_numpy", &from_numpy<float>, "Create from numpy array")
        .def("to_numpy", &to_numpy<float>, "Convert to numpy array")
        .def("__add__", [](const Matrix<float>& a, const Matrix<float>& b) {
            return a + b;
        })
        .def("__mul__", [](const Matrix<float>& a, const Matrix<float>& b) {
            return a * b;
        })
        .def("transpose", &Matrix<float>::transpose)
        .def("print", &Matrix<float>::print)
        .def("matmul_parallel", &Matrix<float>::multiply_parallel, py::arg("other"), py::arg("num_threads") = 0)
        .def_property_readonly("shape", [](const Matrix<float>& mat) {
            return py::make_tuple(mat.rows(), mat.cols());
        });
    
    // 绑定Matrix<double>
    py::class_<Matrix<double>>(m, "MatrixDouble")
        .def(py::init<size_t, size_t>())
        .def_static("from_numpy", &from_numpy<double>, "Create from numpy array")
        .def("to_numpy", &to_numpy<double>, "Convert to numpy array")
        .def("__add__", [](const Matrix<double>& a, const Matrix<double>& b) {
            return a + b;
        })
        .def("__mul__", [](const Matrix<double>& a, const Matrix<double>& b) {
            return a * b;
        })
        .def("transpose", &Matrix<double>::transpose)
        .def("print", &Matrix<double>::print)
        .def("matmul_parallel", &Matrix<double>::multiply_parallel, py::arg("other"), py::arg("num_threads") = 0)
        .def_property_readonly("shape", [](const Matrix<double>& mat) {
            return py::make_tuple(mat.rows(), mat.cols());
        });
    
    // 性能测试函数
    m.def("benchmark_multiplication", [](size_t size) -> py::dict {
        py::dict results;
        
        // 创建随机矩阵
        Matrix<float> a(size, size);
        Matrix<float> b(size, size);
        
        // 填充随机值
        for (size_t i = 0; i < size; ++i) {
            for (size_t j = 0; j < size; ++j) {
                a(i, j) = static_cast<float>(rand()) / RAND_MAX;
                b(i, j) = static_cast<float>(rand()) / RAND_MAX;
            }
        }
        
        // 计时
        auto start = std::chrono::high_resolution_clock::now();
        auto c = a * b;
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        results["size"] = size;
        results["time_ms"] = duration.count();
        results["gflops"] = (2.0 * size * size * size) / (duration.count() * 1e6);  // 理论GFLOPS
        
        return results;
    });
}