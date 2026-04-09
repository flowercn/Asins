#!/bin/bash
cd /home/v/桌面/PyMatrixProject

echo "🚀 优化构建开始..."

# 清理
rm -rf build_opt && mkdir build_opt && cd build_opt

# 检测CPU核心数
CORES=$(nproc --all)
echo "检测到 $CORES 个CPU核心"

# 获取CPU支持的指令集
CPU_FLAGS=$(gcc -march=native -E -v - </dev/null 2>&1 | grep "march")
echo "CPU指令集: $CPU_FLAGS"

# 优化配置
# 注意：在虚拟机中，CMake的SIMD检测可能会误判（检测到编译器支持但CPU不支持AVX512）
# 所以这里关闭USE_SIMD，完全依赖 -march=native 让编译器自动检测当前CPU特性
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DUSE_SIMD=OFF \
    -DUSE_OPENMP=ON \
    -DCMAKE_CXX_FLAGS="-march=native -O3 -ffast-math -funroll-loops -flto" \
    -DCMAKE_EXE_LINKER_FLAGS="-flto"

# 并行编译
make -j$CORES

echo "✅ 优化构建完成！"
echo ""
echo "性能测试命令："
echo "  cd /home/v/桌面/PyMatrixProject/examples && python3 benchmark.py"
