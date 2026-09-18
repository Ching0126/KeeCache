cmake_minimum_required(VERSION 3.10)
#表明cmake的最低版本要求

project(KeeCache)
#项目名称

set(CMAKE_CXX_STANDARD 17)
#//C++标准设置为C++17

set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
#//强制要求使用C++17标准

add_executable(KeeCache main.cpp)
#添加可执行文件KeeCache，源文件为main.cpp
