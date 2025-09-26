#include "common.hpp"

extern RDMA server;

// 首先在服务端开启监听
int listen_on_port (int port);

// 在创建 qp 后即可将 mr 发布接收请求
int post_recv ();

// 服务端接受客户端的链接, 并将本地 mr 的 raddr 和 rkey 发给客户端
int accept ();

// 通过 cq 轮询收到客户端的数据
string receive ();

// 解析客户端远程写在本端的简易报文
void read_local();
