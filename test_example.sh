#!/bin/bash

echo "=== RDMA示例代码测试脚本 ==="
echo ""

# 检查是否在build目录中
if [ ! -f "./main" ]; then
    echo "错误：请在build目录中运行此脚本"
    echo "或者先编译项目：mkdir build && cd build && cmake .. && make"
    exit 1
fi

echo "1. 显示程序帮助信息"
echo "-------------------"
./main
echo ""

echo "2. 测试客户端模式帮助"
echo "-------------------"
./main client
echo ""

echo "3. 测试服务器模式帮助"
echo "-------------------"
./main server
echo ""

echo "4. 测试无效参数"
echo "-------------"
./main invalid_mode
echo ""

echo "=== 测试完成 ==="
echo ""
echo "实际运行说明："
echo "1. 在一个节点上运行: ./main server [IP] [PORT]"
echo "2. 在另一个节点上运行: ./main client [SERVER_IP] [PORT]"
echo "3. 确保两个节点在同一RDMA网络中"
echo ""
echo "默认配置："
echo "- 服务端IP: 10.0.0.53"
echo "- 客户端IP: 10.0.0.55" 
echo "- 端口: 65533"
echo ""
echo "自定义配置示例："
echo "./main server 192.168.100.225 52000"
echo "./main client 192.168.100.223 52000"
