cmake_minimum_required(VERSION 3.10)
project(rdma_project)

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

# 添加可执行文件
add_executable(main main.cpp client.cpp server.cpp)

# 链接RDMA库到目标
target_link_libraries(main rdmacm ibverbs mlx5 -g)
# 一定要链接mlx5库！不然 ibv_reg_mr 会出现段错误
