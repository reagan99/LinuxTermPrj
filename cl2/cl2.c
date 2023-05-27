#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define DEFAULT_PORT 4321
#define BUFFER_SIZE 1024

void receive_file(int sockfd, const char *filename) {
    // 파일 수신 코드 생략

    printf("File received: %s\n", filename);
}

int main(int argc, char *argv[]) {
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char filename[BUFFER_SIZE];
    int port;

    // 포트 번호 확인
    if (argc > 1) {
        port = atoi(argv[1]);
    } else {
        port = DEFAULT_PORT;
    }

    // 소켓 생성
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    // 서버 주소 설정
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
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

            if (send(sockfd, buffer, strlen(buffer), 0) < 0) {
                perror("Failed to send file request");
                break;
            }

            receive_file(sockfd, filename);
        } else {  // 일반 메시지 전송
            if (send(sockfd, buffer, strlen(buffer), 0) < 0) {
                perror("Failed to send message");
                break;
            }

            // 서버로부터 메시지 수신 및 출력
            int bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received < 0) {
                perror("Failed to receive message");
                break;
            } else if (bytes_received == 0) {
                printf("Server closed the connection\n");
                break;
            } else {
                buffer[bytes_received] = '\0';
                printf("Received message from server: %s\n", buffer);
            }
        }
    }

    // 소켓 닫기
    close(sockfd);

    return 0;
}
