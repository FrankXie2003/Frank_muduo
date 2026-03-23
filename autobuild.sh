#!/bin/bash

set -e

# 获取项目根目录的绝对路径
PROJECT_DIR=$(pwd)

# 创建 build 目录（如果不存在）
if [ ! -d "$PROJECT_DIR/build" ]; then
    mkdir "$PROJECT_DIR/build"
fi

# 清空 build 目录
rm -rf "$PROJECT_DIR/build"/*

# 进入 build 目录，执行 cmake 和 make
cd "$PROJECT_DIR/build" &&
    cmake .. &&
    make

# 回到项目根目录
cd "$PROJECT_DIR"

# 创建头文件安装目录（如果不存在）
if [ ! -d '/usr/include/mymuduo' ]; then
    sudo mkdir -p /usr/include/mymuduo
fi

# 拷贝头文件到系统目录
for header in $(ls include/*.h); do
    sudo cp "$header" /usr/include/mymuduo/
done

# 拷贝库文件到系统目录
sudo cp "$PROJECT_DIR/lib/libFrank_muduo.so" /usr/lib/

# 更新动态链接库缓存
sudo ldconfig