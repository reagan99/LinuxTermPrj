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

#define MAX_FILENAME_LEN 12
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Chat Module");

static struct socket *server_socket = NULL;
static struct socket *client_socket = NULL; // 추가: 클라이언트 소켓

static int create_server_socket(int port)
{
    struct sockaddr_in addr;
    int err;

    err = sock_create_kern(&init_net, AF_INET, SOCK_STREAM, IPPROTO_TCP, &server_socket);
    if (err < 0) {
        pr_err("Failed to create server socket: %d\n", err);
        return err;
    }

    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    err = kernel_bind(server_socket, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
    if (err < 0) {
        pr_err("Failed to bind server socket: %d\n", err);
        sock_release(server_socket);
        server_socket = NULL;
        return err;
    }

    err = kernel_listen(server_socket, 1);
    if (err < 0) {
        pr_err("Failed to listen on server socket: %d\n", err);
        sock_release(server_socket);
        server_socket = NULL;
        return err;
    }

    pr_info("Server socket created and bound to port %d\n", port);

    return 0;
}

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
    msg.msg_flags = 0;  // 모든 데이터를 한 번에 수신
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    iov_iter_kvec(&iter, READ, &iov, 1, buffer_size);
    msg.msg_iter = iter;

    bytes_received = kernel_recvmsg(socket, &msg, &iov, 1, buffer_size, msg.msg_flags);

    if (bytes_received < 0) {
        pr_err("Failed to receive message(server): %d\n", bytes_received);
        // 오류 처리를 수행하는 코드 추가
        // 예를 들어, 연결을 종료할 수 있습니다.
        return -1;
    }

    // 수신 메시지를 로그에 출력
    pr_info("Received message: %.*s\n", bytes_received, buffer);

    return bytes_received;
}

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

static int send_file(struct socket *socket, const char *filename) {
    struct file *file;
    loff_t pos = 0;
    char *buffer;
    int bytes_sent = 0;
    int bytes_read;

    file = filp_open(filename, O_RDONLY, 0);
    if (IS_ERR(file)) {
        pr_err("Failed to open file: %.*s\n", MAX_FILENAME_LEN, filename);
        return -1;
    }

    buffer = kmalloc(PAGE_SIZE, GFP_KERNEL);
    if (!buffer) {
        pr_err("Failed to allocate memory for file read\n");
        filp_close(file, NULL);
        return -1;
    }

    while ((bytes_read = kernel_read(file, buffer, PAGE_SIZE, &pos)) > 0) {
        bytes_sent = send_message(socket, buffer, bytes_read);
        if (bytes_sent < 0) {
            pr_err("Failed to send file data\n");
            break;
        }
    }

    kfree(buffer);
    filp_close(file, NULL);

    return bytes_sent;
}


static int __init tcp_socket_init(void)
{
    int err;
    int port = 4321;
    char buffer[100];
    bool is_file_request = false;
    char filename[MAX_FILENAME_LEN] = "my_file.txt";
    int bytes_sent;
    
    err = create_server_socket(port);
    if (err < 0) {
        pr_err("Failed to create server socket: %d\n", err);
        return err;
    }

    while (1) {
        // 클라이언트의 연결 수락
        err = kernel_accept(server_socket, &client_socket, 0);
        if (err < 0) {
            pr_err("Failed to accept client connection: %d\n", err);
            break;
        }

        pr_info("Accepted client connection\n");

        while (1) {
            int bytes_received = receive_message(client_socket, buffer, sizeof(buffer));

            if (bytes_received < 0) {
                pr_err("Failed to receive message(server_init): %d\n", bytes_received);
                break;
            }

            pr_info("Received message: %.*s\n", bytes_received, buffer);

            if (strncmp(buffer, "FILE:", 5) == 0) {
                // 파일 전송 요청 처리
                memset(filename, 0, MAX_FILENAME_LEN);
                strncpy(filename, buffer + 5, MAX_FILENAME_LEN - 1);

                bytes_sent = send_file(client_socket, filename);

                if (bytes_sent < 0) {
                    pr_err("Failed to send file: %.*s\n", MAX_FILENAME_LEN, filename);
                    break;
                }

                pr_info("File sent: %s, Bytes sent: %d\n", filename, bytes_sent);
                is_file_request = true;
            } else {
                // 일반 메시지 전송
                if (is_file_request) {
                    // 파일 전송 후 첫 메시지인 경우에만 "Hello, client!" 메시지를 전송
                    char message[30] = "Hello, client!";
                    bytes_sent = send_message(client_socket, message, strlen(message));

                    if (bytes_sent < 0) {
                        pr_err("Failed to send message: %d\n", bytes_sent);
                        break;
                    }
                    is_file_request = false;
                }
            }
        }

        // 클라이언트 소켓 해제
        if (client_socket) {
            sock_release(client_socket);
            client_socket = NULL;
        }
    }

    return 0;
}

static void __exit tcp_socket_exit(void)
{
    if (server_socket) {
        sock_release(server_socket);
        server_socket = NULL;
    }
    if (client_socket) {
        sock_release(client_socket);
        client_socket = NULL;
    }
}

module_init(tcp_socket_init);
module_exit(tcp_socket_exit);
