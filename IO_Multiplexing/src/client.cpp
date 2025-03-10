#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1" // 使用 localhost 作为服务器 IP
#define PORT 9999

void start_client() {
    // 创建套接字
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return;
    }

    // 确定服务器的 IP 和端口
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server_address.sin_addr) <= 0) {
        perror("Invalid address");
        close(sock);
        return;
    }

    if (connect(sock, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("Connection failed");
        close(sock);
        return;
    }

    std::cout << "Connected to the server." << std::endl;

    std::string message;
    char buffer[1024] = { 0 };

    while (true) {
        std::cout << "Enter message (type 'exit' to quit): ";
        std::getline(std::cin, message);

        if (message == "exit") break;

        if (send(sock, message.c_str(), message.size(), 0) < 0) {
            std::cerr << "Failed to send message." << std::endl;
            break;
        }

        int valread = recv(sock, buffer, sizeof(buffer), 0);
        if (valread > 0) {
            std::cout << "Server reply: " << std::string(buffer, valread) << std::endl;
        }
        else if (valread == 0) {
            std::cout << "Server has closed the connection." << std::endl;
            break;
        }
        else {
            perror("Error receiving data");
            break;
        }
    }

    close(sock);
    std::cout << "Client disconnected." << std::endl;
}

