#include "client.hpp"

RDMA client;

int connect_to_ip_on_port (const char* ip, int port)
{
    // 1. 创建事件通道
    client.ec = rdma_create_event_channel();
    if (!client.ec) return 1;
    
    // 2. 创建cm_id_
    int ret = rdma_create_id(client.ec, &client.id, nullptr, RDMA_PS_TCP);
    if (ret) return 2;

    // 3. 解析目标地址
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port); // 服务器端口
    inet_pton(AF_INET, ip, &addr.sin_addr); // 服务器IP
    ret = rdma_resolve_addr(client.id, nullptr, (sockaddr*)&addr, 2000);
    if (ret) return 3;

    // 等待地址解析完成事件
    rdma_cm_event* event = nullptr;
    ret = rdma_get_cm_event(client.ec, &event);
    if (event->event != RDMA_CM_EVENT_ADDR_RESOLVED) {
        cout << "地址解析未完成，事件: " << rdma_event_str(event->event) << endl;
        rdma_ack_cm_event(event);
        return 3;
    }
    rdma_ack_cm_event(event);

    // 4. 解析路由
    ret = rdma_resolve_route(client.id, 2000);
    if (ret) return 4;

    // 等待路由解析完成事件
    ret = rdma_get_cm_event(client.ec, &event);
    if (event->event != RDMA_CM_EVENT_ROUTE_RESOLVED) {
        cout << "路由解析未完成，事件: " << rdma_event_str(event->event) << endl;
        rdma_ack_cm_event(event);
        return 4;
    }
    rdma_ack_cm_event(event);

    // 5. 注册pd
    client.pd = ibv_alloc_pd(client.id->verbs);
    if (!client.pd) return 5;

    // 6. 注册mr
    int length = MR_SIZE;
    void* local_addr = malloc(length);
    client.mr = ibv_reg_mr(client.pd, local_addr, length, 
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_ATOMIC);
    if (!client.mr) return 6;

    // 7. 注册cq
    client.cq = ibv_create_cq(client.id->verbs, CQE, NULL, NULL, 0);
    if (!client.cq) return 7;
    
    // 8. 注册qp
    ibv_qp_init_attr qp_init_attr;
    memset(&qp_init_attr, 0, sizeof(qp_init_attr));
    qp_init_attr.cap.max_send_wr = MAX_SEND_WR;
    qp_init_attr.cap.max_recv_wr = 1;
    qp_init_attr.cap.max_send_sge = 1;
    qp_init_attr.cap.max_recv_sge = 1;
    qp_init_attr.qp_context = &client;
    qp_init_attr.sq_sig_all = 0;
    qp_init_attr.qp_type = IBV_QPT_RC;  // 这个标志表示 QP 维护的是可靠连接
    qp_init_attr.send_cq = client.cq;
    qp_init_attr.recv_cq = client.cq;
    ret = rdma_create_qp(client.id, client.pd, &qp_init_attr);
    client.qp = client.id->qp;
    if (ret) return 8;

    // 9. 提交接收请求
    ibv_recv_wr wr;
    ibv_recv_wr* bad_wr = nullptr;
    ibv_sge sge;
    memset(&sge, 0, sizeof(sge));
    sge.addr = (uintptr_t)client.mr->addr;
    sge.length = BUFFER_SIZE;
    sge.lkey = client.mr->lkey;
    memset(&wr, 0, sizeof(wr));
    wr.wr_id = 0;
    wr.next = nullptr;
    wr.sg_list = &sge;
    wr.num_sge = 1;
    ret = ibv_post_recv(client.id->qp, &wr, &bad_wr);
    if (ret) return 9;

    // 10. 发起连接请求
    exchange_data data;   // 在接受连接时将 rkey 和 raddr 给服务端，以便服务端在后续 ibv_post_send 中填写
    data.rkey = client.mr->rkey;
    data.raddr = reinterpret_cast<uint64_t>(client.mr->addr); // mr基址，使用时可以加偏移量

    rdma_conn_param conn_param;
    memset(&conn_param, 0, sizeof(conn_param));
    conn_param.responder_resources = 1;
    conn_param.initiator_depth = 1;
    conn_param.retry_count = 3;
    conn_param.private_data = &data;
    conn_param.private_data_len = sizeof(data);
    ret = rdma_connect(client.id, &conn_param);
    if (ret) return 10;

    // 等待连接建立事件
    ret = rdma_get_cm_event(client.ec, &event);
    if (event->event != RDMA_CM_EVENT_ESTABLISHED) {
        cout << "连接未建立，事件: " << rdma_event_str(event->event) << endl;
        rdma_ack_cm_event(event);
        return 10;
    }
    if (!event->param.conn.private_data || event->param.conn.private_data_len == 0) {
        cout << "服务端建链时未传输 rkey 和 raddr" << endl;
        return 6;
    }
    exchange_data remote_data;
    memcpy(&remote_data, event->param.conn.private_data, sizeof(exchange_data));  // 这一步操作很危险，容易出现空指针
    client.rkey_on_remote = remote_data.rkey;
    client.raddr_on_remote = remote_data.raddr;

    rdma_ack_cm_event(event);
    return 0;
}

int send (string message)
{
    std::vector<uint8_t> data(message.begin(), message.end());
    memcpy(client.mr->addr, data.data(), data.size());
    ibv_sge send_sge;
    ibv_send_wr send_wr;
    ibv_send_wr* bad_wr = nullptr;

    memset(&send_sge, 0, sizeof(send_sge));
    send_sge.addr = reinterpret_cast<uintptr_t>(client.mr->addr);
    send_sge.length = data.size();
    send_sge.lkey = client.mr->lkey;

    /*
     * wr_send 操作不需要填写远端地址或秘钥
     * 只需要填写 qp 即可 send / recv
     * 建链时创建的 qp 就好比打电话
     * 电话接通后两个 qp 之间稳定连接就不需要反复填地址了
     */
    memset(&send_wr, 0, sizeof(send_wr));
    send_wr.wr_id = ++client.wr_id;  // 同一片 mr 被重复使用时推荐用递增 id
    send_wr.opcode = IBV_WR_SEND;
    send_wr.send_flags = IBV_SEND_SIGNALED;
    send_wr.sg_list = &send_sge;
    send_wr.num_sge = 1;

    int ret = ibv_post_send(client.qp, &send_wr, &bad_wr);
    if (ret) return ret;
    return 0;
}

int write (string message)  // 约定前4个字节为报文头, 且只填写报文长度信息
{
    std::vector<uint8_t> data(message.begin(), message.end());
    // 在缓冲区开头写入数据长度（4字节）
    uint32_t data_length = data.size();
    memcpy(client.mr->addr, &data_length, sizeof(data_length));
    
    // 在长度后面写入实际数据
    memcpy(static_cast<char*>(client.mr->addr) + sizeof(data_length), 
           data.data(), data.size());
    uint32_t total_length = sizeof(data_length) + data.size();

    ibv_sge send_sge;
    ibv_send_wr send_wr;
    ibv_send_wr* bad_wr = nullptr;

    memset(&send_sge, 0, sizeof(send_sge));
    send_sge.addr = reinterpret_cast<uintptr_t>(client.mr->addr);
    send_sge.length = total_length;
    send_sge.lkey = client.mr->lkey;

    memset(&send_wr, 0, sizeof(send_wr));
    send_wr.wr_id = reinterpret_cast<uintptr_t>(client.mr->addr); // write时推荐用addr
    send_wr.opcode = IBV_WR_RDMA_WRITE;
    send_wr.send_flags = IBV_SEND_SIGNALED;
    send_wr.sg_list = &send_sge;
    send_wr.num_sge = 1;
    send_wr.wr.rdma.remote_addr = client.raddr_on_remote;
    send_wr.wr.rdma.rkey = client.rkey_on_remote;  // write 操作要填写远端地址和秘钥
    int ret = ibv_post_send(client.qp, &send_wr, &bad_wr);
    if (ret) return ret;
    return 0;
}

string read_remote ()  // 直接将指定 remote_addr 里的数据搬到本地 mr 里
{
    ibv_sge send_sge;
    ibv_send_wr send_wr;
    ibv_send_wr* bad_wr = nullptr;

    memset(&send_sge, 0, sizeof(send_sge));
    send_sge.addr = reinterpret_cast<uintptr_t>(client.mr->addr);
    send_sge.length = client.mr->length;
    send_sge.lkey = client.mr->lkey;

    memset(&send_wr, 0, sizeof(send_wr));
    send_wr.wr_id = reinterpret_cast<uintptr_t>(client.mr->addr); // 
    send_wr.opcode = IBV_WR_RDMA_READ;
    send_wr.send_flags = IBV_SEND_SIGNALED;
    send_wr.sg_list = &send_sge;
    send_wr.num_sge = 1;
    send_wr.wr.rdma.remote_addr = client.raddr_on_remote;
    send_wr.wr.rdma.rkey = client.rkey_on_remote;  // RDMA read 操作也要填写远端地址和秘钥
    int ret = ibv_post_send(client.qp, &send_wr, &bad_wr);

    char* data = static_cast<char*>(client.mr->addr);
    uint32_t data_length;
    memcpy(&data_length, data, sizeof(data_length));
    string message(data + sizeof(data_length), data_length);
    return message;
}
