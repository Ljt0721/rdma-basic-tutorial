#ifndef COMMON_HPP
#define COMMON_HPP

#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <rdma/rdma_verbs.h>
#include <rdma/rdma_cma.h>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>

#define CQE 8192
#define MAX_SEND_WR 8192
#define BUFFER_SIZE 1024
#define MR_SIZE 1024

// 默认配置 - 可以在运行时通过命令行参数覆盖
#define DEFAULT_PORT 65533
#define DEFAULT_SERVER_IP "10.0.0.53"
#define DEFAULT_CLIENT_IP "10.0.0.55"

using namespace std;

struct RDMA {
    rdma_event_channel* ec = nullptr;
    rdma_cm_id* listen_id = nullptr;
    rdma_cm_id* id = nullptr;
    ibv_pd* pd = nullptr;
    ibv_mr* mr = nullptr;
    ibv_cq* cq = nullptr;
    ibv_qp* qp = nullptr;
    uint32_t rkey_on_remote = 0;
    uint64_t raddr_on_remote = 0;
    uint64_t wr_id = 0;
};

struct exchange_data {
    uint32_t rkey;
    uint64_t raddr;
};

#endif // COMMON_HPP
