#ifndef MATRIX_HPP
#define MATRIX_HPP

#include <memory>
#include <vector>
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <immintrin.h>  // SIMD指令集
#include <thread>       // 线程支持

template<typename T>
class Matrix {
private:
    std::unique_ptr<T[]> data;
    size_t rows_;
    size_t cols_;
    
    // 内存对齐分配（对齐到64字节，用于SIMD）
    static std::unique_ptr<T[]> allocate_aligned(size_t size) {
        if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>) {
            void* ptr = nullptr;
            if constexpr (alignof(T) <= 64) {
                if (posix_memalign(&ptr, 64, size * sizeof(T)) != 0) {
                    throw std::bad_alloc();
                }
            } else {
                if (posix_memalign(&ptr, alignof(T), size * sizeof(T)) != 0) {
                    throw std::bad_alloc();
                }
            }
            return std::unique_ptr<T[]>(static_cast<T*>(ptr));
        } else {
            return std::make_unique<T[]>(size);
        }
    }
    
public:
    // 构造函数
    Matrix(size_t rows, size_t cols) : rows_(rows), cols_(cols) {
        if (rows == 0 || cols == 0) {
            throw std::invalid_argument("Matrix dimensions must be positive");
        }
        data = allocate_aligned(rows * cols);
        std::fill(data.get(), data.get() + rows * cols, T{});
    }
    
    // 移动语义
    Matrix(Matrix&& other) noexcept 
        : data(std::move(other.data)), rows_(other.rows_), cols_(other.cols_) {
        other.rows_ = 0;
        other.cols_ = 0;
    }
    
    Matrix& operator=(Matrix&& other) noexcept {
        if (this != &other) {
            data = std::move(other.data);
            rows_ = other.rows_;
            cols_ = other.cols_;
            other.rows_ = 0;
            other.cols_ = 0;
        }
        return *this;
    }
    
    // 禁止拷贝（使用工厂方法或clone）
    Matrix(const Matrix&) = delete;
    Matrix& operator=(const Matrix&) = delete;
    
    // 工厂方法：从向量创建
    static Matrix from_vector(size_t rows, size_t cols, const std::vector<T>& values) {
        if (values.size() != rows * cols) {
            throw std::invalid_argument("Vector size must match matrix dimensions");
        }
        Matrix mat(rows, cols);
        std::copy(values.begin(), values.end(), mat.data.get());
        return mat;
    }
    
    // 获取维度
    size_t rows() const { return rows_; }
    size_t cols() const { return cols_; }
    
    // 元素访问（行主序）
    T& operator()(size_t row, size_t col) {
        return data[row * cols_ + col];
    }
    
    const T& operator()(size_t row, size_t col) const {
        return data[row * cols_ + col];
    }
    
    // 矩阵加法
    Matrix operator+(const Matrix& other) const {
        if (rows_ != other.rows_ || cols_ != other.cols_) {
            throw std::invalid_argument("Matrix dimensions must match for addition");
        }
        
        Matrix result(rows_, cols_);
        size_t total = rows_ * cols_;
        
        // SIMD优化（如果可用）
        if constexpr (std::is_same_v<T, float>) {
            #ifdef __AVX2__
            size_t i = 0;
            for (; i + 7 < total; i += 8) {
                __m256 a = _mm256_load_ps(&data[i]);
                __m256 b = _mm256_load_ps(&other.data[i]);
                __m256 c = _mm256_add_ps(a, b);
                _mm256_store_ps(&result.data[i], c);
            }
            for (; i < total; ++i) {
                result.data[i] = data[i] + other.data[i];
            }
            #else
            for (size_t i = 0; i < total; ++i) {
                result.data[i] = data[i] + other.data[i];
            }
            #endif
        } else {
            for (size_t i = 0; i < total; ++i) {
                result.data[i] = data[i] + other.data[i];
            }
        }
        
        return result;
    }
    
    // 矩阵乘法（优化版本）
    Matrix operator*(const Matrix& other) const {
        if (cols_ != other.rows_) {
            throw std::invalid_argument("Matrix dimensions incompatible for multiplication");
        }
        
        Matrix result(rows_, other.cols_);
        
        // 性能优化：循环重排 + 局部性优化
        if constexpr (std::is_same_v<T, float>) {
            // 分块优化（提高缓存命中率）
            constexpr size_t BLOCK_SIZE = 64;
            for (size_t i = 0; i < rows_; i += BLOCK_SIZE) {
                for (size_t j = 0; j < other.cols_; j += BLOCK_SIZE) {
                    for (size_t k = 0; k < cols_; k += BLOCK_SIZE) {
                        // 计算当前分块
                        size_t i_end = std::min(i + BLOCK_SIZE, rows_);
                        size_t j_end = std::min(j + BLOCK_SIZE, other.cols_);
                        size_t k_end = std::min(k + BLOCK_SIZE, cols_);
                        
                        for (size_t ii = i; ii < i_end; ++ii) {
                            for (size_t kk = k; kk < k_end; ++kk) {
                                T a = (*this)(ii, kk);
                                for (size_t jj = j; jj < j_end; ++jj) {
                                    result(ii, jj) += a * other(kk, jj);
                                }
                            }
                        }
                    }
                }
            }
        } else {
            // 基础实现
            for (size_t i = 0; i < rows_; ++i) {
                for (size_t k = 0; k < cols_; ++k) {
                    T a = (*this)(i, k);
                    for (size_t j = 0; j < other.cols_; ++j) {
                        result(i, j) += a * other(k, j);
                    }
                }
            }
        }
        
        return result;
    }
    
    // 矩阵转置
    Matrix transpose() const {
        Matrix result(cols_, rows_);
        for (size_t i = 0; i < rows_; ++i) {
            for (size_t j = 0; j < cols_; ++j) {
                result(j, i) = (*this)(i, j);
            }
        }
        return result;
    }
    
    // 打印矩阵
    void print() const {
        for (size_t i = 0; i < rows_; ++i) {
            for (size_t j = 0; j < cols_; ++j) {
                std::cout << (*this)(i, j) << " ";
            }
            std::cout << std::endl;
        }
    }

    // 并行计算（使用OpenMP）
    Matrix multiply_parallel(const Matrix& other, int num_threads = 0) const {
        if (cols_ != other.rows_) {
            throw std::invalid_argument("Matrix dimensions incompatible");
        }
        
        Matrix result(rows_, other.cols_);
        
        // 设置线程数
        if (num_threads <= 0) {
            num_threads = std::thread::hardware_concurrency();
        }
        
        #ifdef _OPENMP
        #pragma omp parallel num_threads(num_threads)
        {
            // 每个线程处理连续的行块，避免伪共享
            #pragma omp for schedule(static) nowait
            for (size_t i = 0; i < rows_; ++i) {
                // 为每个线程分配独立的行，避免缓存竞争
                for (size_t j = 0; j < other.cols_; ++j) {
                    T sum = T{};
                    // 内层循环展开（提高ILP）
                    size_t k = 0;
                    for (; k + 3 < cols_; k += 4) {
                        sum += (*this)(i, k) * other(k, j) +
                               (*this)(i, k+1) * other(k+1, j) +
                               (*this)(i, k+2) * other(k+2, j) +
                               (*this)(i, k+3) * other(k+3, j);
                    }
                    // 处理剩余部分
                    for (; k < cols_; ++k) {
                        sum += (*this)(i, k) * other(k, j);
                    }
                    result(i, j) = sum;
                }
            }
        }
        #else
        // 如果没有OpenMP，回退到串行
        result = (*this) * other;
        #endif
        
        return result;
    }
};

// 特殊化：支持double类型的SIMD
template<>
Matrix<double>::Matrix(size_t rows, size_t cols) : rows_(rows), cols_(cols) {
    if (rows == 0 || cols == 0) {
        throw std::invalid_argument("Matrix dimensions must be positive");
    }
    data = allocate_aligned(rows * cols);
    std::fill(data.get(), data.get() + rows * cols, 0.0);
}

#endif // MATRIX_HPP