#!/bin/bash

set -e

# 创建构建目录
mkdir -p build
cd build

# 配置项目
echo "Configuring project..."
cmake -DCMAKE_BUILD_TYPE=Release ..

# 编译项目
echo "Building project..."
make -j$(nproc)

echo "Build completed successfully!"
echo "The executable is located at: $(pwd)/bin/chat_server"