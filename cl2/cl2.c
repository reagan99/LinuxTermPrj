#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 4320
#define BUFFER_SIZE 1024

void receive_file(int sockfd, const char *filename) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to open file for writing");
        return;
    }

    char buffer[BUFFER_SIZE];
    int bytes_received;

    // 파일 데이터 수신
    while ((bytes_received = recv(sockfd, buffer, sizeof(buffer), 0)) > 0) {
        fwrite(buffer, 1, bytes_received, file);
    }

    fclose(file);

    if (bytes_received < 0) {
        perror("Failed to receive file");
        return;
    }

    printf("File received: %s\n", filename);
}
int receive_message(int sockfd, char *buffer, size_t buffer_size) {
    int bytes_received = recv(sockfd, buffer, buffer_size, 0);
    if (bytes_received < 0) {
        perror("Failed to receive message");
        return -1;
    } else if (bytes_received == 0) {
        printf("Connection closed by server\n");
        return -1;
    } else {
        printf("Received message: %.*s\n", bytes_received, buffer);
        return bytes_received;
    }
}

int send_message(int sockfd, const char *message) {
    int bytes_sent = send(sockfd, message, strlen(message), 0);
    if (bytes_sent < 0) {
        perror("Failed to send message");
        return -1;
    }
    return bytes_sent;
}

int main() {
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char filename[BUFFER_SIZE];

    // 소켓 생성
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    // 서버 주소 설정
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid server IP address");
        exit(EXIT_FAILURE);
    }

    // 서버에 연결
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to connect to server");
        exit(EXIT_FAILURE);
    }

    // 메시지 전송 및 파일 수신
    while (1) {
        // 메시지 입력
        printf("Enter your message (or file name to receive, 'exit' to quit):\n");
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = '\0';  // 개행 문자 제거

        // "exit" 입력 시 종료
        if (strcmp(buffer, "exit") == 0) {
            break;
        }

        // 파일 수신 요청인 경우
        if (strncmp(buffer, "FILE:", 5) == 0) {
            memset(filename, 0, sizeof(filename));
            strncpy(filename, buffer + 5, sizeof(filename) - 1);

            if (send_message(sockfd, buffer) < 0) {
                break;
            }

            receive_file(sockfd, filename);
        } else {  // 일반 메시지 전송
            if (send_message(sockfd, buffer) < 0) {
                break;
            }

            // 메시지 수신
            receive_message(sockfd, buffer, sizeof(buffer));
        }
    }

    // 소켓 닫기
    close(sockfd);

    return 0;
}
