#!/bin/bash
# 一键构建脚本

set -e  # 遇到错误退出

echo "🚀 PyMatrix 构建开始..."

# 清理旧构建
if [ -d "build" ]; then
    echo "清理旧构建..."
    rm -rf build
fi

# 创建构建目录
mkdir -p build
cd build

# 配置
echo "配置CMake..."
if command -v ninja &> /dev/null; then
    cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DUSE_SIMD=OFF
else
    cmake .. -DCMAKE_BUILD_TYPE=Release -DUSE_SIMD=OFF
fi

# 获取CPU核心数
if command -v nproc &> /dev/null; then
    CORES=$(nproc)
elif command -v sysctl &> /dev/null; then
    CORES=$(sysctl -n hw.ncpu)
else
    CORES=4
fi

# 编译
echo "编译（使用 $CORES 个核心）..."
cmake --build . --config Release --parallel $CORES

# 运行测试
echo "运行C++测试..."
if [ -f "./bin/simple_test" ]; then
    ./bin/simple_test
fi

# 运行基准测试
# echo "运行性能测试..."
# if [ -f "./bin/benchmark" ]; then
#     ./bin/benchmark
# fi

# 安装Python模块
echo "安装Python模块..."
python3 install_module.py

echo "✅ 构建完成！"
echo ""
echo "使用说明："
echo "1. Python中导入: import pymatrix"
echo "2. 运行演示: python3 examples/simple_demo.py"
echo "3. 清理构建: rm -rf build"