#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/net.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/uaccess.h>
#include <net/sock.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>

#define MAX_FILENAME_LEN 12
#define MAX_MESSAGE_LEN 100

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Chat Module");

static struct socket *server_socket = NULL;
static struct socket *client_socket = NULL;
static struct sock *netlink_socket = NULL;
#define NETLINK_USER 31

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
    msg.msg_flags = 0;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    iov_iter_kvec(&iter, READ, &iov, 1, buffer_size);
    msg.msg_iter = iter;

    bytes_received = kernel_recvmsg(socket, &msg, &iov, 1, buffer_size, msg.msg_flags);

    if (bytes_received < 0) {
        pr_err("Failed to receive message(server): %d\n", bytes_received);
        return -1;
    }

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

static int send_file(struct socket *socket, const char *filename)
{
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

static void send_to_user(char *message, size_t message_len)
{
    struct sk_buff *skb;
    struct nlmsghdr *nlh;
    int pid;
    int res;

    skb = nlmsg_new(message_len, GFP_KERNEL);
    if (!skb) {
        pr_err("Failed to allocate netlink message buffer\n");
        return;
    }

    nlh = nlmsg_put(skb, 0, 0, NLMSG_DONE, message_len, 0);
    memcpy(nlmsg_data(nlh), message, message_len);

    pid = 0; // 0 means to send to all listeners
    res = nlmsg_unicast(netlink_socket, skb, pid);
    if (res < 0) {
        pr_err("Failed to send netlink message: %d\n", res);
        nlmsg_free(skb);
    }
}

static void netlink_receive(struct sk_buff *skb)
{
    struct nlmsghdr *nlh;
    char *message;
    size_t message_len;

    nlh = nlmsg_hdr(skb);
    message = (char *)nlmsg_data(nlh);
    message_len = nlmsg_len(nlh);

    int bytes_sent = send_message(client_socket, message, message_len);

    // 클라이언트로 메시지 전송
    if (bytes_sent < 0) {
        pr_err("Failed to send message: %d\n", bytes_sent);
    }
}


static struct netlink_kernel_cfg netlink_cfg = {
    .input = netlink_receive,
};

static int __init tcp_socket_init(void)
{
    int err;
    int port = 4321;
    char buffer[MAX_MESSAGE_LEN];
    bool is_file_request = false;
    char filename[MAX_FILENAME_LEN] = "my_file.txt";
    //int bytes_sent;

    err = create_server_socket(port);
    if (err < 0) {
        pr_err("Failed to create server socket: %d\n", err);
        return err;
    }

    // Create netlink socket
    netlink_socket = netlink_kernel_create(&init_net, NETLINK_USER, &netlink_cfg);
    if (!netlink_socket) {
        pr_err("Failed to create netlink socket\n");
        return -ENOMEM;
    }

    // Wait for client connection
    err = kernel_accept(server_socket, &client_socket, O_NONBLOCK);
    if (err < 0) {
        pr_err("Failed to accept client connection: %d\n", err);
        return err;
    }

    while (!kthread_should_stop()) {
        // Receive message from the client
        int bytes_received = receive_message(client_socket, buffer, sizeof(buffer));
        if (bytes_received > 0) {
            // Check if it's a file request
            if (strncmp(buffer, "file:", 5) == 0) {
                is_file_request = true;
                strncpy(filename, buffer + 5, sizeof(filename));
            } else {
                is_file_request = false;
            }

            // Print the received message
            pr_info("Received message: %s\n", buffer);

            // Send the message to the user
            send_to_user(buffer, bytes_received);

            // Send a file if it's a file request
            if (is_file_request) {
                int bytes_sent = send_file(client_socket, filename);
                if (bytes_sent < 0) {
                    pr_err("Failed to send file: %d\n", bytes_sent);
                }
            }
        }
    }

    return 0;
}

static void __exit tcp_socket_exit(void)
{
    if (client_socket) {
        kernel_sock_shutdown(client_socket, SHUT_RDWR);
        sock_release(client_socket);
    }

    if (server_socket) {
        kernel_sock_shutdown(server_socket, SHUT_RDWR);
        sock_release(server_socket);
    }

    if (netlink_socket) {
        netlink_kernel_release(netlink_socket);
    }
}

module_init(tcp_socket_init);
module_exit(tcp_socket_exit);

