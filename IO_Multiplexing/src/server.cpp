#include <iostream>
#include <thread>
#include <vector>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include "../head/thread_pool.h" // 自定义线程池头文件

#define PORT 9999              // 定义服务器监听端口号
#define MAX_EVENTS 1024        // epoll 最大监听事件数
#define HIGH_LOAD_THRESHOLD 100 // 高负载阈值，用于动态选择触发模式

// 设置文件描述符为非阻塞模式
void set_nonblocking(int socket_fd) {
    int flags = fcntl(socket_fd, F_GETFL, 0);          // 获取当前文件描述符标志
    fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);    // 设置为非阻塞模式
}

// 动态选择触发模式（水平触发 LT 或边缘触发 ET）
uint32_t choose_trigger_mode(int current_load) {
    if (current_load < HIGH_LOAD_THRESHOLD) {
        // 低负载下使用水平触发模式（LT）
        std::cout << "[INFO] Using Level Triggered (LT) mode. Current load: " << current_load << std::endl;
        return EPOLLIN;
    }
    else {
        // 高负载下使用边缘触发模式（ET）
        std::cout << "[INFO] Using Edge Triggered (ET) mode. Current load: " << current_load << std::endl;
        return EPOLLIN | EPOLLET;
    }
}

// 处理客户端数据的函数
void handle_client(int client_socket) {
    char buffer[1024];
    while (true) {
        // 清空缓冲区以接收新数据
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0); // 从客户端接收数据

        if (bytes_received < 0) {
            // 如果 recv 返回值为负数，判断是否为非阻塞模式下的无数据可读状态
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break; // 数据已读取完毕，退出循环
            }
            else {
                // 其他错误，关闭客户端连接
                perror("Receive failed");
                close(client_socket);
                return;
            }
        }
        else if (bytes_received == 0) {
            // 如果 recv 返回值为 0，表示客户端已断开连接
            std::cout << "Client disconnected: " << client_socket << std::endl;
            close(client_socket);
            return;
        }

        // 打印客户端发送的数据
        std::cout << "Client (" << client_socket << ") message: " << buffer << std::endl;

        // 构造回送消息并发送给客户端
        std::string response = "Server received: " + std::string(buffer);
        send(client_socket, response.c_str(), response.size(), 0);
    }
}

// 启动服务器
void start_server() {
    // 创建服务器套接字
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        return;
    }

    // 设置服务器套接字为非阻塞模式
    set_nonblocking(server_socket);

    // 配置服务器地址结构
    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;              // 使用 IPv4 地址
    server_address.sin_addr.s_addr = INADDR_ANY;      // 监听所有可用网络接口
    server_address.sin_port = htons(PORT);           // 设置端口号，使用网络字节序

    // 绑定套接字到指定地址和端口
    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        perror("Bind failed");
        close(server_socket);
        return;
    }

    // 开始监听连接请求
    if (listen(server_socket, 5) == -1) {
        perror("Listen failed");
        close(server_socket);
        return;
    }

    std::cout << "Server listening on port " << PORT << "..." << std::endl;

    // 创建 epoll 实例
    int epoll_fd = epoll_create1(0);        
    if (epoll_fd == -1) {
        perror("Epoll creation failed");
        close(server_socket);
        return;
    }

    // 将服务器套接字添加到 epoll 实例
    epoll_event server_event{};                     //创建epoll_evert结构体 
    server_event.data.fd = server_socket;           // 关联服务器套接字
    server_event.events = EPOLLIN;                  // 默认使用水平触发模式（LT）
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &server_event) == -1) {
        perror("Epoll control failed");
        close(server_socket);
        return;
    }

    // 创建线程池
    Thread_Pool thread_pool(std::thread::hardware_concurrency());

    // 定义事件数组，用于存储 epoll_wait 的返回事件
    epoll_event events[MAX_EVENTS];

    while (true) {
        // 等待事件发生
        int event_count = epoll_wait(epoll_fd, events, MAX_EVENTS, -1); 
        if (event_count < 0) {
            perror("Epoll wait failed");
            break;
        }

        for (int i = 0; i < event_count; ++i) {
            int event_fd = events[i].data.fd;   

            if (event_fd == server_socket) {
                // 处理新客户端连接
                while (true) {
                    sockaddr_in client_address{};
                    socklen_t client_len = sizeof(client_address);
                    int client_socket = accept(server_socket, (struct sockaddr*)&client_address, &client_len);

                    if (client_socket < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break; // 已处理完所有挂起的连接请求
                        }
                        else {
                            perror("Accept failed");
                            continue;
                        }
                    }

                    // 打印新连接信息
                    std::cout << "New connection from " << inet_ntoa(client_address.sin_addr)
                        << ":" << ntohs(client_address.sin_port) << std::endl;

                    // 设置客户端套接字为非阻塞模式
                    set_nonblocking(client_socket);

                    // 动态选择触发模式并添加到 epoll
                    epoll_event client_event{};
                    client_event.data.fd = client_socket; // 关联客户端套接字
                    client_event.events = choose_trigger_mode(thread_pool.get_task_count());
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_socket, &client_event);
                }
            }
            else if (events[i].events & EPOLLIN) {
                // 客户端有数据可读，交由线程池处理
                thread_pool.enqueue(handle_client, event_fd);
            }
        }
    }

    close(server_socket); // 关闭服务器套接字
}
