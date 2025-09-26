# RDMA 基础示例代码

这是一个最基础的双节点单向收发读写的RDMA示例学习代码，支持包括CM建链、verbs收发、RDMA远程读写等核心功能。
保证让只懂最基础的 C++ 语言的萌新也能快速入门RDMA的全部核心功能实现

## 功能特性

- **CM建链**：使用RDMA CM (Connection Manager) 建立可靠连接
- **Verbs收发**：基于IB Verbs的Send/Recv操作
- **RDMA远程读写**：对端无感知的RDMA Write和RDMA Read操作
- **命令行参数支持**：灵活配置IP地址和端口号


## 快速开始

### 编译项目

```bash
mkdir build && cd build
cmake ..
make
```

### 运行示例

**服务端模式**：

```bash
# 使用默认配置（IP: 10.0.0.53, 端口: 65533）
./main server

# 指定IP和端口
./main server 192.168.100.225 52000
```

**客户端模式**：

```bash
# 使用默认配置（连接至10.0.0.53:65533）
./main client

# 指定目标服务器IP和端口
./main client 192.168.100.223 52000
```

### 运行顺序

1. 首先在一个节点上启动服务端：`./main server`
2. 然后在另一个节点上启动客户端：`./main client`

## 代码结构

```
rdma_test/
├── main.cpp          # 主程序，支持命令行参数
├── client.cpp        # 客户端实现
├── client.hpp        # 客户端头文件
├── server.cpp        # 服务端实现
├── server.hpp        # 服务端头文件
├── common.hpp        # 公共定义和结构体
├── CMakeLists.txt    # 构建配置
└── README.md         # 本文档
```

## 核心API流程

### 1. CM建链流程

**服务端**：

- `rdma_create_event_channel()` - 创建事件通道
- `rdma_create_id()` - 创建CM ID
- `rdma_bind_addr()` - 绑定地址
- `rdma_listen()` - 开始监听
- `rdma_accept()` - 接受连接

**客户端**：

- `rdma_create_event_channel()` - 创建事件通道
- `rdma_create_id()` - 创建CM ID
- `rdma_resolve_addr()` - 解析地址
- `rdma_resolve_route()` - 解析路由
- `rdma_connect()` - 发起连接

### 2. Verbs资源创建

- `ibv_alloc_pd()` - 分配保护域
- `ibv_reg_mr()` - 注册内存区域
- `ibv_create_cq()` - 创建完成队列
- `rdma_create_qp()` - 创建队列对

### 3. 数据传输操作

**Send/Recv操作**：

- `ibv_post_send()` - 提交发送请求
- `ibv_post_recv()` - 提交接收请求
- `ibv_poll_cq()` - 轮询完成队列

**RDMA操作**：

- `IBV_WR_RDMA_WRITE` - RDMA写操作
- `IBV_WR_RDMA_READ` - RDMA读操作

## 运行前提

### 硬件要求

- 至少两个支持RDMA的网络节点
- 每个节点必须配备InfiniBand网卡或支持RDMA over Converged Ethernet (RoCE)的网卡

### 软件要求

- Linux操作系统
- 已安装RDMA驱动和用户态库：
  - `librdmacm` (RDMA连接管理库)
  - `libibverbs` (Verbs接口库)
  - `libmlx5` (Mellanox驱动库)

### 网络配置

- 两个节点必须在同一RDMA网络中
- 可以是物理机、虚拟机或容器环境
- 确保防火墙允许指定的端口通信

## 深入学习资源

### 概念理解

- [知乎专栏：RDMA技术详解](https://zhuanlan.zhihu.com/p/164908617) - 从概念到应用的完整讲解
- [RDMA通信流程解析](https://zhuanlan.zhihu.com/p/481256797) - 具体API的详细讲解

### API文档

(ibv_poll_cq为例)
- [IBM官方文档](https://www.ibm.com/docs/zh/aix/7.3.0?topic=management-ibv-poll-cq)
- [RDMA Mojo](https://www.rdmamojo.com/2013/02/15/ibv_poll_cq/)

### CMake 教程

(如果是算法大佬, 还不懂 cpp 项目的构建可自学)
- [CMake 入门实战](https://github.com/wzpan/cmake-demo/tree/master)

### 思维导图

如果不想阅读长篇专栏文章，可以直接查看思维导图：

- `rdma_mindmap.jpg` - 图片格式
- `rdma_mindmap.xmind` - XMind格式

这些思维导图总结了RDMA的核心概念和API使用流程。

## 扩展建议

### 1. 双向读写

当前代码是单向的（客户端→服务端），要实现双向读写：

- 客户端将自己的`raddr`和`rkey`通过Send操作发送给服务端
- 服务端收到后即可对客户端进行RDMA读写操作

### 2. 多节点分布式RDMA

要实现多节点通信：

- 设计节点管理机制
- 高ID节点向低ID节点发起建链申请
- 最终形成全连接的RDMA网络拓扑

### 3. 性能优化

- 使用多QP并行传输
- 实现零拷贝数据传输
- 优化CQ轮询策略
- 使用事件驱动模式

## 代码特点

这个示例代码的特点是：

1. **简洁明了**：只包含最核心的RDMA操作
2. **易于理解**：每个函数都有详细的注释
3. **可扩展性强**：代码结构清晰，便于添加新功能
4. **实用性强**：可以直接在实际环境中运行

## 总结

RDMA应用起来其实就是填写参数和调用API，非常好理解。跟着这个示例代码敲一遍，就基本跑通了所有RDMA重要的操作。读者在理解代码后可以自行编写更复杂的RDMA应用。

记住：**实践是最好的学习方式**，多动手调试，观察每个API的返回值和状态变化，就能深入理解RDMA的工作原理。

```

```
