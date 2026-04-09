import numpy as np
import time
import pymatrix

def benchmark_numpy_vs_pymatrix(size=512):
    """对比NumPy和我们的C++扩展的性能"""
    
    print(f"\n{'='*60}")
    print(f"性能对比测试: 矩阵大小 {size}×{size}")
    print('='*60)
    
    # 生成随机矩阵
    np.random.seed(42)
    a_np = np.random.randn(size, size).astype(np.float32)
    b_np = np.random.randn(size, size).astype(np.float32)
    
    # 1. NumPy测试
    print("\n1. NumPy (OpenBLAS/MKL) 性能:")
    start = time.time()
    c_np = np.dot(a_np, b_np)
    np_time = time.time() - start
    print(f"   时间: {np_time:.4f} 秒")
    print(f"   性能: {(2 * size**3) / (np_time * 1e9):.2f} GFLOPS")
    
    # 2. 我们的C++扩展测试
    print("\n2. PyMatrix (C++ SIMD) 性能:")
    
    # 转换到我们的矩阵格式
    a_cpp = pymatrix.MatrixFloat.from_numpy(a_np)
    b_cpp = pymatrix.MatrixFloat.from_numpy(b_np)
    
    start = time.time()
    c_cpp = a_cpp * b_cpp
    cpp_time = time.time() - start
    
    print(f"   时间: {cpp_time:.4f} 秒")
    print(f"   性能: {(2 * size**3) / (cpp_time * 1e9):.2f} GFLOPS")
    
    # 3. 验证结果正确性
    c_cpp_np = c_cpp.to_numpy()
    error = np.abs(c_np - c_cpp_np).max()
    print(f"\n3. 结果验证:")
    print(f"   最大误差: {error:.6e}")
    print(f"   {'✓ 结果正确' if error < 1e-4 else '✗ 结果有误'}")
    
    # 4. 加速比
    speedup = np_time / cpp_time
    print(f"\n4. 加速比: {speedup:.2f}x")
    if speedup > 1:
        print(f"   PyMatrix比NumPy快 {speedup:.2f} 倍!")
    else:
        print(f"   NumPy比PyMatrix快 {1/speedup:.2f} 倍")
    
    return {
        'numpy_time': np_time,
        'cpp_time': cpp_time,
        'speedup': speedup,
        'error': error
    }

def demo_basic_operations():
    """演示基本操作"""
    print("\n基本操作演示:")
    print("-" * 40)
    
    # 创建矩阵 (通过numpy)
    a_np = np.array([[1, 2, 3], [4, 5, 6]], dtype=np.float32)
    b_np = a_np * 2
    
    a = pymatrix.MatrixFloat.from_numpy(a_np)
    b = pymatrix.MatrixFloat.from_numpy(b_np)
    
    # 填充值 (在实际中应该通过numpy)
    # for i in range(2):
    #     for j in range(3):
    #         a(i, j) = i * 3 + j + 1
    #         b(i, j) = (i * 3 + j + 1) * 2
    
    print("矩阵 A:")
    a.print()
    
    print("\n矩阵 B:")
    b.print()
    
    print("\nA + B:")
    c = a + b
    c.print()
    
    # 矩阵乘法演示
    print("\n矩阵乘法演示:")
    m1 = pymatrix.MatrixFloat.from_numpy(np.array([[1, 2], [3, 4]], dtype=np.float32))
    m2 = pymatrix.MatrixFloat.from_numpy(np.array([[5, 6], [7, 8]], dtype=np.float32))
    
    print("M1:")
    m1.print()
    
    print("\nM2:")
    m2.print()
    
    print("\nM1 × M2:")
    result = m1 * m2
    result.print()
    
    # 验证结果
    expected = np.array([[19, 22], [43, 50]], dtype=np.float32)
    actual = result.to_numpy()
    print(f"\n验证: {'✓ 正确' if np.allclose(expected, actual) else '✗ 错误'}")

def test_concurrent_performance():
    """多线程性能测试"""
    import pymatrix
    import numpy as np
    import time
    
    size = 512
    print(f"   测试矩阵大小: {size}x{size}")
    
    a = pymatrix.MatrixFloat.from_numpy(np.random.randn(size, size).astype(np.float32))
    b = pymatrix.MatrixFloat.from_numpy(np.random.randn(size, size).astype(np.float32))
    
    # 单线程
    start = time.time()
    c1 = a * b
    t1 = time.time() - start
    print(f"   单线程用时: {t1:.4f}s")
    
    # 多线程 (OpenMP)
    try:
        start = time.time()
        c2 = a.matmul_parallel(b)
        t2 = time.time() - start
        print(f"   多线程用时: {t2:.4f}s")
        print(f"   加速比: {t1/t2:.2f}x")
    except AttributeError:
        print("   多线程方法未找到 (可能未编译OpenMP支持)")

def test_advanced_features():
    """测试高级功能"""
    import pymatrix
    import numpy as np
    import time
    
    print("\n高级功能测试:")
    print("-" * 40)
    
    # 1. 批量操作
    print("1. 批量矩阵乘法测试...")
    batch_size = 10
    matrices_a = [pymatrix.MatrixFloat.from_numpy(np.random.randn(64, 64).astype(np.float32)) 
                  for _ in range(batch_size)]
    matrices_b = [pymatrix.MatrixFloat.from_numpy(np.random.randn(64, 64).astype(np.float32)) 
                  for _ in range(batch_size)]
    
    start = time.time()
    results = [a * b for a, b in zip(matrices_a, matrices_b)]
    batch_time = time.time() - start
    
    print(f"   批量处理 {batch_size} 个64×64矩阵用时: {batch_time:.3f}秒")
    print(f"   平均每个: {batch_time/batch_size*1000:.1f}ms")
    
    # 2. 内存使用测试
    print("\n2. 内存使用测试...")
    try:
        import psutil
        import os
        
        process = psutil.Process(os.getpid())
        mem_before = process.memory_info().rss / 1024 / 1024  # MB
        
        # 创建大矩阵
        big_mat = pymatrix.MatrixFloat(1000, 1000)
        mem_after = process.memory_info().rss / 1024 / 1024
        
        print(f"   创建1000×1000矩阵前内存: {mem_before:.1f}MB")
        print(f"   创建后内存: {mem_after:.1f}MB")
        print(f"   矩阵占用: {mem_after - mem_before:.1f}MB")
        print(f"   理论值: {1000*1000*4/1024/1024:.1f}MB (float32)")
    except ImportError:
        print("   psutil未安装，跳过内存测试")
    
    # 3. 并发测试
    print("\n3. 多线程性能测试...")
    test_concurrent_performance()

def scalability_test():
    """可扩展性测试"""
    print("\n可扩展性测试:")
    print("-" * 40)
    
    sizes = [64, 128, 256, 512]
    
    for size in sizes:
        print(f"\n测试大小: {size}×{size}")
        try:
            # 使用C++内部的benchmark函数
            results = pymatrix.benchmark_multiplication(size)
            print(f"  时间: {results['time_ms']:.1f} ms")
            print(f"  性能: {results['gflops']:.2f} GFLOPS")
        except Exception as e:
            print(f"  错误: {e}")

def main():
    """主函数"""
    print("PyMatrix Accelerator - C++高性能矩阵计算库")
    print("=" * 60)
    
    # 1. 演示基本操作
    demo_basic_operations()

    # 2. 高级功能测试
    test_advanced_features()
    
    # 3. 可扩展性测试
    scalability_test()
    
    # 3. 性能对比测试
    print("\n" + "=" * 60)
    print("与NumPy的性能对比")
    print("=" * 60)
    
    # 小规模测试
    benchmark_numpy_vs_pymatrix(128)
    
    # 中规模测试（根据你的硬件调整）
    print("\n提示: 大规模测试可能需要较多内存和计算时间")
    choice = input("是否运行512x512的性能测试? (y/n): ")
    if choice.lower() == 'y':
        benchmark_numpy_vs_pymatrix(512)
    
    print("\n" + "=" * 60)
    print("测试完成!")
    print("=" * 60)

if __name__ == "__main__":
    main()