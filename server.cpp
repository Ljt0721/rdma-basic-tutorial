#include "common.hpp"
#include "server.hpp"

RDMA server;

int listen_on_port (int port)
{
    // 1. 创建事件通道
    server.ec = rdma_create_event_channel();
    if (!server.ec) return 1;

    // 2. 创建cm_id_
    int ret = rdma_create_id(server.ec, &server.listen_id, nullptr, RDMA_PS_TCP);
    if (ret) return 2;

    // 3. 根据cm_id_绑定服务器的指定端口
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    ret = rdma_bind_addr(server.listen_id, (struct sockaddr*)&addr);
    if (ret) return 3;

    // 4. 在指定端口持续监听有无客户端的连接请求
    ret = rdma_listen(server.listen_id, 6);
    if (ret) return 4;
    return 0;
}

int post_recv ()
{
    ibv_sge recv_sge;
    ibv_recv_wr recv_wr;
    ibv_recv_wr* bad_wr = nullptr;

    memset(&recv_sge, 0, sizeof(recv_sge));
    recv_sge.addr = reinterpret_cast<uintptr_t>(server.mr->addr);
    recv_sge.length = server.mr->length;
    recv_sge.lkey = server.mr->lkey;

    memset(&recv_wr, 0, sizeof(recv_wr));
    recv_wr.wr_id = ++server.wr_id;  // 简单使用累加器即可
    recv_wr.sg_list = &recv_sge;
    recv_wr.num_sge = 1;

    int ret = ibv_post_recv(server.qp, &recv_wr, &bad_wr);
    if (ret) return ret;

    return 0;
}

int accept ()
{
    // 1. 收到客户端的RDMA_CM_EVENT_CONNECT_REQUEST事件
    rdma_cm_event* event = nullptr;
    int ret = rdma_get_cm_event(server.ec, &event);
    if (event->event != RDMA_CM_EVENT_CONNECT_REQUEST) {
        cout << "收到非连接请求的事件";
        return 1;
    }
    /*
     * 一定要用客户端给的 event 里的 cm_id 创建 pd qp 等等
     * 不要用监听时创建的 cm_id ！
     */
    server.id = event->id; 

    // 2. 注册 pd
    server.pd = ibv_alloc_pd(server.id->verbs);
    if (!server.pd) return 2;

    // 3. 注册 mr
    int length = MR_SIZE;
    void* local_addr = malloc(length);
    server.mr = ibv_reg_mr(server.pd, local_addr, length, 
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_ATOMIC);
    if (!server.mr) return 3;

    // 4. 注册cq
    server.cq = ibv_create_cq(server.id->verbs, CQE, NULL, NULL, 0);
    if (!server.cq) return 4;
    
    // 5. 注册qp
    ibv_qp_init_attr qp_init_attr;
    memset(&qp_init_attr, 0, sizeof(qp_init_attr));
    qp_init_attr.cap.max_send_wr = MAX_SEND_WR;
    qp_init_attr.cap.max_recv_wr = 1;
    qp_init_attr.cap.max_send_sge = 1;
    qp_init_attr.cap.max_recv_sge = 1;
    qp_init_attr.qp_context = &server;
    qp_init_attr.sq_sig_all = 0;
    qp_init_attr.qp_type = IBV_QPT_RC;
    qp_init_attr.send_cq = server.cq;
    qp_init_attr.recv_cq = server.cq;
    ret = rdma_create_qp(server.id, server.pd, &qp_init_attr);
    server.qp = server.id->qp;
    if (ret) return 5;

    /*
     * 因不知客户端的 send 请求何时来
     * 服务端在注册完 qp 后, 建立连接前, 即可立马提交 recv 申请
     * 以免通信时出现 RNR 问题 (receiver-not-ready)
     */
    ret = post_recv();
    if (ret) return 5;

    // 6. 接受连接
    exchange_data data;   // 在接受连接时将 rkey 和 raddr 给客户端，以便客户端在后续 ibv_post_send 中填写
    data.rkey = server.mr->rkey;
    data.raddr = reinterpret_cast<uint64_t>(server.mr->addr);  // local_addr == server.mr->addr

    rdma_conn_param conn_param;
    memset(&conn_param, 0, sizeof(conn_param));
    conn_param.initiator_depth = 1;
    conn_param.retry_count = 10;
    conn_param.private_data = &data;
    conn_param.private_data_len = sizeof(data);
    ret = rdma_accept(server.id, &conn_param);
    if (ret) return 6;

    // 等待连接建立事件
    ret = rdma_get_cm_event(server.ec, &event);
    if (event->event != RDMA_CM_EVENT_ESTABLISHED) {
        cout << "未成功接收，事件：" << rdma_event_str(event->event) << endl;
        rdma_ack_cm_event(event);
        return 6; 
    }

    rdma_ack_cm_event(event);
    return 0;
}

string receive ()
{
    int num_entries = 1; // 最多返回一个 cqe
    ibv_wc wc;
    int ret;
    int timeout_counter = 0;
    const int max_timeout_count = 10000;

    // 轮询完成队列, 等待接收完成
    do {
        ret = ibv_poll_cq(server.cq, num_entries, &wc);
        if (ret < 0) return "轮询完成队列失败"; // 轮询完成队列失败
        if (ret == 0) {
            this_thread::sleep_for(chrono::microseconds(500));  // 没返回事件就短暂休眠避免忙等待
            timeout_counter++;
            if (timeout_counter > max_timeout_count)
                return "接收操作超时";
            continue;
        }
        // 处理完成事件类型
        if (wc.status != IBV_WC_SUCCESS)
            return "接收操作失败";
        if (wc.opcode == IBV_WC_RECV) // 找到RECV事件，跳出循环处理
            break;
        else { // 非RECV事件则继续轮询
            cout << "忽略非RECV完成事件: " << wc.opcode << endl;
            continue;
        }
    } while (true);

    cout << "接收完成： ";
    std::vector<uint8_t> data(static_cast<uint8_t*>(server.mr->addr),
                           static_cast<uint8_t*>(server.mr->addr) + wc.byte_len);
    string message = string(data.begin(), data.end());

    ret = post_recv();  // 接收完数据后重新提交接收申请以等待下一次客户端 send
    if (ret) return "重新发布接收申请失败";

    return message;
}

void read_local()
{
    char* buffer = static_cast<char*>(server.mr->addr);

    // 先读取前4字节获取数据长度
    uint32_t data_length;
    memcpy(&data_length, buffer, sizeof(data_length));

    // 根据长度读取实际数据
    string received_message(buffer + sizeof(data_length), data_length);
    cout << "接收数据： " << received_message << endl;
    cout << "数据长度： " << data_length << endl;
}
