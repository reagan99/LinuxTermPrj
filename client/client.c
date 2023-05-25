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

#define SERVER_PORT 4321
#define MAX_MESSAGE_LEN 50
#define MAX_FILENAME_LEN 256
#define MAX_BUFFER_SIZE 512

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Chat Module - Client");

static struct socket *client_socket = NULL;

static int connect_to_server(void)
{
    struct sockaddr_in addr;
    int err;

    err = sock_create_kern(&init_net, AF_INET, SOCK_STREAM, IPPROTO_TCP, &client_socket);
    if (err < 0) {
        pr_err("Failed to create client socket: %d\n", err);
        return err;
    }

    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    err = kernel_connect(client_socket, (struct sockaddr *)&addr, sizeof(struct sockaddr_in), 0);
    if (err < 0) {
        pr_err("Failed to connect to the server: %d\n", err);
        sock_release(client_socket);
        client_socket = NULL;
        return err;
    }

    pr_info("Connected to the server\n");

    return 0;
}

static int send_message(struct socket *socket, const char *message, size_t message_len)
{
    struct msghdr msg = {};
    struct kvec iov;
    ssize_t bytes_sent;

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


static int receive_message(struct socket *socket, char *buffer, size_t buffer_size)
{
    struct msghdr msg = {};
    struct kvec iov;
    struct iov_iter iter;
    ssize_t bytes_received;

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
        pr_err("Failed to receive message(client): %ld\n", bytes_received);
        return bytes_received;
    }

    pr_info("Received message from server: %s\n", buffer);

    return bytes_received;
}





static int receive_file(struct socket *socket, const char *filename)
{
    struct file *file;
    loff_t offset = 0;
    ssize_t bytes_received;
    char *buffer;

    file = filp_open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (!file || IS_ERR(file)) {
        pr_err("Failed to create file: %s\n", filename);
        return -EINVAL;
    }

    buffer = kmalloc(MAX_BUFFER_SIZE, GFP_KERNEL);
    if (!buffer) {
        pr_err("Failed to allocate memory for buffer\n");
        filp_close(file, NULL);
        return -ENOMEM;
    }

    do {
        bytes_received = receive_message(socket, buffer, MAX_BUFFER_SIZE);
        if (bytes_received < 0) {
            pr_err("Failed to receive file data: %ld\n", bytes_received);
            kfree(buffer);
            filp_close(file, NULL);
            return bytes_received;
        }
        kernel_write(file, (const char __user *)buffer, bytes_received, &offset);

        offset += bytes_received;
    } while (bytes_received > 0);

    kfree(buffer);
    filp_close(file, NULL);
    return 0;
}

static int __init tcp_socket_client_init(void)
{
    int err;
    char receive_filename[] = "fileee.txt";
    char message[MAX_MESSAGE_LEN];
    char receive_buffer[MAX_MESSAGE_LEN];
    err = connect_to_server();
    if (err < 0) {
        pr_err("Failed to connect to server\n");
        return err;
    }

    // 채팅 기능 추가
    
    snprintf(message, sizeof(message), "Hello, server!");
    err = send_message(client_socket, message, strlen(message));
    if (err < 0) {
        pr_err("Failed to send message: %d\n", err);
        goto exit;
    }

    pr_info("Message sent successfully\n");

    
    err = receive_message(client_socket, receive_buffer, sizeof(receive_buffer));
    if (err < 0) {
        pr_err("Failed to receive message: %d\n", err);
        goto exit;
    }

    pr_info("Received message from server: %s\n", receive_buffer);

    // 파일 수신 추가
    err = receive_file(client_socket, receive_filename);
    if (err < 0) {
        pr_err("Failed to receive file: %d\n", err);
        goto exit;
    }

    pr_info("File received successfully\n");

exit:
    if (client_socket) {
        sock_release(client_socket);
        client_socket = NULL;
    }

    return err;
}

static void __exit tcp_socket_client_exit(void)
{
    if (client_socket) {
        sock_release(client_socket);
        client_socket = NULL;
    }
}

module_init(tcp_socket_client_init);
module_exit(tcp_socket_client_exit);
























