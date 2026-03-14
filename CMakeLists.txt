cmake_minimum_required(VERSION 3.0)
project(Frank_muduo)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall")

# 设置头文件目录
include_directories(${PROJECT_SOURCE_DIR}/include)

# 设置库输出目录
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)

# 收集所有源文件
aux_source_directory(src SRC_LIST)

# 编译成共享库
add_library(Frank_muduo SHARED ${SRC_LIST})

# 编译测试程序
add_subdirectory(testmuduo)