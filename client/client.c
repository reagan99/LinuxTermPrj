#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/net.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <net/sock.h>
#include <linux/fs.h>
#include <linux/slab.h>

#define SERVER_PORT 12345
#define MAX_MESSAGE_LEN 50

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Chat Module - Client");

// 클라이언트 소켓
static struct socket *client_socket = NULL;

// 서버에 연결
static int connect_to_server(void)
{
    struct sockaddr_in addr;
    int err;

    // 소켓 생성
    err = sock_create_kern(&init_net, AF_INET, SOCK_STREAM, IPPROTO_TCP, &client_socket);
    if (err < 0) {
        pr_err("Failed to create client socket: %d\n", err);
        return err;
    }

    // 서버 주소 설정
    // 소켓 주소 설정
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // 0.0.0.0

    // 소켓에 포트 바인딩
    err = kernel_bind(server_socket, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
    if (err < 0) {
        pr_err("Failed to bind server socket: %d\n", err);
        sock_release(server_socket);
        server_socket = NULL;
        return err;
    }


    pr_info("Connected to the server\n");

    return 0;
}

// 메시지 송신
static int send_message(struct socket *socket, const char *message, size_t message_len)
{
    struct msghdr msg = {};
    struct kvec iov;
    int bytes_sent;

    iov.iov_base = (void *)message;
    iov.iov_len = message_len;

    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags = 0;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    iov_iter_kvec(&msg.msg_iter, WRITE, &iov, 1, message_len);

    bytes_sent = kernel_sendmsg(socket, &msg, &iov, 1, message_len);
    return bytes_sent;
}

// 메시지 수신
static int receive_message(struct socket *socket, char *buffer, size_t buffer_size)
{
    struct msghdr msg = {};
    struct kvec iov;
    struct iov_iter iter;
    int bytes_received;

    iov.iov_base = buffer;
    iov.iov_len = buffer_size;

    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags = 0;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    iov_iter_kvec(&iter, READ, &iov, 1, buffer_size);
    msg.msg_iter = iter;

    bytes_received = kernel_recvmsg(socket, &msg, &iov, 1, buffer_size, msg.msg_flags);
    if (bytes_received < 0) {
        pr_err("Failed to receive message(client): %d\n", bytes_received);
        return bytes_received;
    }

    // 수신한 메시지를 로그에 출력
    pr_info("Received message from server: %.*s\n", bytes_received, buffer);

    return bytes_received;
}

static int __init tcp_socket_client_init(void)
{
    int err;
    char buffer[MAX_MESSAGE_LEN];

    // 서버에 연결
    err = connect_to_server();
    if (err < 0) {
        pr_err("Failed to connect to server\n");
        return err;
    }

    // 메시지 주고받기 예시
    while (1) {
        // 서버로 메시지 전송
        char message[] = "Hello, server!";
        int bytes_sent = send_message(client_socket, message, strlen(message));
        int bytes_received = receive_message(client_socket, buffer, sizeof(buffer));
        if (bytes_sent < 0) {
            pr_err("Failed to send message: %d\n", bytes_sent);
            break;
        }

        // 서버로부터 메시지 수신
        
        if (bytes_received < 0) {
            pr_err("Failed to receive message(client): %d\n", bytes_received);
            break;
        }

        // 수신한 메시지 출력
        pr_info("Received message from server: %s\n", buffer);
    }

    return 0;
}

static void __exit tcp_socket_client_exit(void)
{
    // 클라이언트 소켓 해제
    if (client_socket) {
        sock_release(client_socket);
        client_socket = NULL;
    }
}

module_init(tcp_socket_client_init);
module_exit(tcp_socket_client_exit);
