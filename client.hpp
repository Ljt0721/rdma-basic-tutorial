#include "common.hpp"

extern RDMA client;

// cm 建链流程
int connect_to_ip_on_port (const char* ip, int port);

// verbs 发流程
int send (string message);

// RDMA remote write 对端无感知
int write (string message);

// RDMA remote read 对端无感知
string read_remote ();
