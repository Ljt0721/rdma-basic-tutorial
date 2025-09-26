#include <iostream>
#include <string>
#include <cstring>
#include "common.hpp"
#include "server.hpp"
#include "client.hpp"

using namespace std;

void print_usage(const char* program_name) {
    cout << "用法: " << program_name << " <mode> [ip] [port]" << endl;
    cout << "模式:" << endl;
    cout << "  client          - 运行客户端模式" << endl;
    cout << "  server          - 运行服务器模式" << endl;
    cout << "参数:" << endl;
    cout << "  ip              - 指定IP地址 (可选)" << endl;
    cout << "  port            - 指定端口号 (可选)" << endl;
    cout << "示例:" << endl;
    cout << "  " << program_name << " client                    - 使用默认配置运行客户端" << endl;
    cout << "  " << program_name << " server                    - 使用默认配置运行服务器" << endl;
    cout << "  " << program_name << " client 192.168.100.223 52000 - 客户端连接到指定IP和端口" << endl;
    cout << "  " << program_name << " server 192.168.100.225 52000 - 服务器在指定IP和端口监听" << endl;
}

int run_client(const char* ip = DEFAULT_SERVER_IP, int port = DEFAULT_PORT) {
    cout << "启动客户端模式" << endl;
    cout << "目标服务器: " << ip << ":" << port << endl;
    
    /**  在服务端开启监听后, 客户端再建立连接  **/
    int ret = connect_to_ip_on_port(ip, port);
    if (ret != 0) {
        cout << "连接建立失败，错误码: " << ret << endl;
        return ret;
    }
    cout << "连接建立成功" << endl;

    /**  发送一个消息  **/
    string message = "Hello from client";
    ret = send(message);
    if (ret != 0) {
        cout << "Send 1 失败，错误码: " << ret << endl;
        return ret;
    }
    cout << "Send 1 成功" << endl;

    this_thread::sleep_for(chrono::milliseconds(200));

    /**  发送第二个消息, 确保对端在接收完后重新发布接收请求  **/
    message = "I'm ljt";
    ret = send(message);
    if (ret != 0) {
        cout << "Send 2 失败，错误码: " << ret << endl;
        return ret;
    }
    cout << "Send 2 成功" << endl;
    
    this_thread::sleep_for(chrono::milliseconds(200));

    /**  远程写, 此时对端没任何感知  **/
    message = "RDMA write";
    ret = write(message);
    if (ret != 0) {
        cout << "Write 失败，错误码: " << ret << endl;
        return ret;
    }
    cout << "Write 成功" << endl;

    /**  远程读, 对端也不会有感知  **/
    message = read_remote();
    cout << "远端数据为: " << message << endl;
    
    return 0;
}

int run_server(const char* ip = DEFAULT_SERVER_IP, int port = DEFAULT_PORT) {
    cout << "启动服务器模式" << endl;
    cout << "监听地址: " << ip << ":" << port << endl;
    
    /**  Server端开启监听  **/
    int ret = listen_on_port(port);
    if (ret != 0) {
        cout << "监听失败，错误码: " << ret << endl;
        return ret;
    }
    cout << "已开始监听客户端 CM 建链请求" << endl;

    /**  Server端accept连接请求  **/
    ret = accept();
    if (ret != 0) {
        cout << "CM建链失败，错误码: " << ret << endl;
        return ret;
    }
    cout << "CM建链成功" << endl;

    /**  Server端接收client send  **/
    string message = receive();
    cout << "接收消息1: " << message << endl;

    this_thread::sleep_for(chrono::microseconds(1000));

    message = receive();
    cout << "接收消息2: " << message << endl;

    /**  Server端直接读client write的数据  **/
    this_thread::sleep_for(chrono::milliseconds(500));
    read_local();

    return 0;
}

int main(int argc, char* argv[]) {
    // 检查参数数量
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    // 解析模式
    string mode = argv[1];
    
    // 解析IP和端口参数
    const char* ip = nullptr;
    int port = 0;
    
    if (argc >= 3) {
        ip = argv[2];
    }
    
    if (argc >= 4) {
        try {
            port = stoi(argv[3]);
            if (port <= 0 || port > 65535) {
                cout << "错误: 端口号必须在1-65535范围内" << endl;
                return 1;
            }
        } catch (const exception& e) {
            cout << "错误: 无效的端口号 '" << argv[3] << "'" << endl;
            return 1;
        }
    }

    // 根据模式运行相应的功能
    if (mode == "client") {
        if (ip == nullptr) {
            ip = DEFAULT_SERVER_IP;
        }
        if (port == 0) {
            port = DEFAULT_PORT;
        }
        return run_client(ip, port);
    } else if (mode == "server") {
        if (ip == nullptr) {
            ip = DEFAULT_SERVER_IP;
        }
        if (port == 0) {
            port = DEFAULT_PORT;
        }
        return run_server(ip, port);
    } else {
        cout << "错误: 未知模式 '" << mode << "'" << endl;
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}
