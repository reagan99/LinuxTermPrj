#include <sys/socket.h>
#include <linux/netlink.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

// Constant
#define NETLINK_USER 31

// Length Limitation
#define MAX_NAME_LENGTH 100
#define MAX_PACKET_LENGTH 200
#define MAX_IPV4_BUFFER_LENGTH 20
#define MAX_IPV4_STRING_LENGTH 16

// Network Configuration
#define PORT 12345

// Type value
#define MSG_REGISTER 1
#define MSG_REGISTER_RESPONSE 2
#define MSG_DEREGISTER 3
#define MSG_DEREGISTER_RESPONSE 4
#define MSG_GET 5
#define MSG_GET_RESPONSE 6
#define MSG_VERIFY 7
#define MSG_VERIFY_RESPONSE 8
#define MSG_TYPE_NETERR 9

// Code value used in kernel message
#define MSG_SUCCESS 0
#define MSG_FAILED 1
#define MSG_KERNEL_NETERR 2

// Message setting macros
#define SET_MSG_CHAR(name, x) (*name = x); (name += 1);
#define SET_MSG_SHORT(name, x) (*(unsigned short *)name = htons(x)); (name += 2);
#define SET_MSG_INTEGER(name, x) (*(unsigned int *)name = htonl(x)); (name += 4);
#define SET_MSG_STRING(name, x) (strcpy(name, x)); (name += strlen(x)+1);

// Maximum payload size
#define MAX_PAYLOAD 1024

// Netlink packet size
#define MSG_ADD_LENGTH 4
#define MSG_DEL_LENGTH 4
#define MSG_GET_LENGTH 8

// Netlink socket variable
int sock_fd;

// Function prototype
unsigned short send_add_message(char*, unsigned int, char*);
unsigned short send_del_message(char*, unsigned int, char*);
unsigned short send_get_message(char*, char*);
unsigned short set_add_message(char*, unsigned int, char*);
unsigned short set_del_message(char*, unsigned int, char*);
unsigned short set_get_message(char*, char*);
unsigned char checksum_generate(char*, int);
bool checksum_verify(char*, int);

int main(void) {

	/* Declaration */

	// Error discrimination codes
	char kernel_failure[2] = {0, MSG_FAILED};
	char net_error[2] = {0, MSG_KERNEL_NETERR};

	// Buffers
	char name[MAX_NAME_LENGTH]; // Name buffer
	char buff_rcv[MAX_PACKET_LENGTH]; // Receiving buffer from client
	char buff_send[MAX_PACKET_LENGTH]; // Sending buffer to client

	// Sockets
	int service_sock_fd; // Server
	struct sockaddr_in server_addr; // Server
	struct sockaddr_in client_addr; // Client
	struct sockaddr_nl src_addr; // Kernel
	
	// Data variables
	unsigned short result; // Error code
	int length; // Total packet length sending to client
	int client_addr_len = sizeof(client_addr);

	// Pointers
	unsigned char* type;
	unsigned short* name_length;
	unsigned char* ipv4_addr;

	/* Initialization */

	// Client service socket creation
	if ((service_sock_fd = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("[ FATAL__ ] Client service socket creation failure.\n");
		return 1;
	}

	// Module netlink socket creation
	if ((sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_USER)) < 0) {
		printf("[ FATAL__ ] Module netlink socket creation failure.\n");
		return 1;
	}

	memset(&server_addr, 0, sizeof(server_addr));
	memset(&src_addr, 0, sizeof(src_addr));

	// Setting up client service socket
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// Setting up module netlink socket
	src_addr.nl_family = AF_NETLINK;
	src_addr.nl_pid = getpid();

	// Binding
	if (bind(service_sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		printf("[ FATAL__ ] Socket binding failure.\n");
		return 1;
	}
	if (bind(sock_fd, (struct sockaddr*)&src_addr, sizeof(src_addr)) < 0) {
		printf("[ FATAL__ ] Socket binding failure.\n");
		return 1;
	}

	/* Packet processing */
	// Receive request from client, and process(get result from kernel and send back to client) received request.

	while(1) {

		printf("[ READY__ ] Waiting for further request.\n\n");

		// Receive packet
		if (recvfrom(service_sock_fd, buff_rcv, MAX_PACKET_LENGTH, 0, (struct sockaddr*)&client_addr, &client_addr_len) < 0) {
			printf("[ NET_ERR ] Client packet receiving failure.\n");
			sendto(service_sock_fd, &net_error, 1, 0, (struct sockaddr*)&client_addr, (socklen_t)client_addr_len);
			printf("[ READY__ ] Waiting for further request.\n\n");
			continue;
		}

		printf("===============================================================\n");
		printf("|  01/04  | Data received.\n");

		// Parse packet
		name_length = (unsigned short*)(buff_rcv + 1);
		memcpy(name, buff_rcv + 3, ntohs(*name_length));
		*(name + ntohs(*name_length)) = 0;
		ipv4_addr = buff_rcv + 3 + ntohs(*name_length);

		// Verify Checksum
		if (!checksum_verify(buff_rcv, ntohs(*name_length))) {
			printf("%x\n", buff_rcv[ntohs(*name_length) + 4 - 1]);
			printf("| NET_ERR | Error in checksum.\n");
			printf("===============================================================\n\n");
			sendto(service_sock_fd, &net_error, 1, 0, (struct sockaddr*)&client_addr, (socklen_t)client_addr_len);
			printf("[ READY__ ] Waiting for further request.\n\n");
			continue;
		}

		// Classify packet
		type = buff_rcv;
		switch(*type) {

			// add
			case MSG_REGISTER: {
				result = send_add_message(name, ipv4_addr[0] << 24 | ipv4_addr[1] << 16 | ipv4_addr[2] << 8 | ipv4_addr[3], buff_send);

				// Kernel failure
				if (result == MSG_FAILED) {
					kernel_failure[0] = MSG_REGISTER_RESPONSE;
					if (sendto(service_sock_fd, kernel_failure, 2, 0, (struct sockaddr*)&client_addr, (socklen_t)client_addr_len) < 0) {
						printf("| NET_ERR | Sending error to client failed.\n");
					}
				}

				// Kernel network error
				else if (result > 0) {
					net_error[0] = MSG_REGISTER_RESPONSE;
					if (sendto(service_sock_fd, net_error, 2, 0, (struct sockaddr*)&client_addr, (socklen_t)client_addr_len) < 0) {
						printf("| NET_ERR | Sending error to client failed.\n");
					}
				}

				// Success
				else {
					length = 9 + ntohs(*(unsigned short*)(buff_send + 2));
					if (sendto(service_sock_fd, buff_send, length, 0, (struct sockaddr*)&client_addr, (socklen_t)client_addr_len) < 0) {
						printf("| NET_ERR | Sending answer to client failed.\n");
					}
				}
				break;
			}

			// del
			case MSG_DEREGISTER: {
				result = send_del_message(name, ipv4_addr[0] << 24 | ipv4_addr[1] << 16 | ipv4_addr[2] << 8 | ipv4_addr[3], buff_send);

				// Kernel failure
				if (result == MSG_FAILED) {
					kernel_failure[0] = MSG_DEREGISTER_RESPONSE;
					if (sendto(service_sock_fd, kernel_failure, 2, 0, (struct sockaddr*)&client_addr, (socklen_t)client_addr_len) < 0) {
						printf("| NET_ERR | Sending error to client failed.\n");
					}
				}

				// Kernel network error
				else if (result > 0) {
					net_error[0] = MSG_DEREGISTER_RESPONSE;
					if (sendto(service_sock_fd, net_error, 2, 0, (struct sockaddr*)&client_addr, (socklen_t)client_addr_len) < 0) {
						printf("| NET_ERR | Sending error to client failed.\n");
					}
				}

				// Success
				else {
					length = 9 + ntohs(*(unsigned short*)(buff_send + 2));
					if (sendto(service_sock_fd, buff_send, length, 0, (struct sockaddr*)&client_addr, (socklen_t)client_addr_len) < 0) {
						printf("| NET_ERR | Sending answer to client failed.\n");
					}
				}
				break;
			}

			// get
			case MSG_GET: {
				result = send_get_message(name, buff_send);

				// Kernel failure
				if (result == MSG_FAILED) {
					kernel_failure[0] = MSG_GET_RESPONSE;
					if (sendto(service_sock_fd, kernel_failure, 2, 0, (struct sockaddr*)&client_addr, (socklen_t)client_addr_len) < 0) {
						printf("| NET_ERR | Sending error to client failed.\n");
					}
				}

				// Kernel network error
				else if (result > 0) {
					net_error[0] = MSG_GET_RESPONSE;
					if (sendto(service_sock_fd, net_error, 2, 0, (struct sockaddr*)&client_addr, (socklen_t)client_addr_len) < 0) {
						printf("| NET_ERR | Sending error to client failed.\n");
					}
				}

				// Success
				else {
					length = 9 + ntohs(*(unsigned short*)(buff_send + 2));
					if (sendto(service_sock_fd, buff_send, length, 0, (struct sockaddr*)&client_addr, (socklen_t)client_addr_len) < 0) {
						printf("| NET_ERR | Sending answer to client failed.\n");
					}
				}
				break;
			}

			// Suspicious message
			default: {
				printf("| NET_ERR | Untranslatable request received.\n");
				net_error[0] = MSG_TYPE_NETERR;
				if (sendto(service_sock_fd, net_error, 2, 0, (struct sockaddr*)&client_addr, (socklen_t)client_addr_len) < 0) {
					printf("| NET_ERR | Sending error to client failed.\n");
				}
				break;
			}

		}

		printf("===============================================================\n\n");

	}

	/* Termination */

	// Socket close
	close(sock_fd);
	close(service_sock_fd);

	return 0;

}

unsigned short send_add_message(char* name, unsigned int ipv4_addr, char* buf_rcv) {
// buf_rcv : response message buffer for client, not netlink
// With this function, you can register entry that has name and ipv4_addr.

	/* Declaration */

	char* d_point;

	unsigned char* type;
	unsigned char* code;
	unsigned short* length;

	unsigned char* type_send = (unsigned char*)buf_rcv;
	unsigned char* code_send = (unsigned char*)(buf_rcv + 1);
	unsigned short* length_send = (unsigned short*)(buf_rcv + 2);
	unsigned char* name_send = (unsigned char*)(buf_rcv + 4);
	unsigned int* ipv4_addr_send = (unsigned int*)(buf_rcv + 4);
	unsigned char* checksum_send = (unsigned char*)(buf_rcv + 8);

	unsigned short rcv_len = (unsigned short)strlen(name);
	struct nlmsghdr *nlh = NULL;
	char buf[NLMSG_SPACE(MAX_PAYLOAD)];

	/* Netlink packet exchange */

	memset(buf, 0, NLMSG_SPACE(MAX_PAYLOAD));

	// Send message
	nlh = (struct nlmsghdr*)buf;
	nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_flags = 0;

	printf("|  02/04  | Sending registration request(%s, %s) to kernel.\n", name, inet_ntoa(*(struct in_addr*)&ipv4_addr));
	set_add_message(name, ipv4_addr, (char*)NLMSG_DATA(nlh));
	send(sock_fd, buf, nlh->nlmsg_len, 0);
	printf("|  03/04  | Waiting for message from kernel.\n");

	// Receive message
	recv(sock_fd, buf, nlh->nlmsg_len, 0);
	d_point = (char*)NLMSG_DATA(nlh);

	/* Process message from kernel */

	// fix pointers
	type = (unsigned char*)d_point;
	code = (unsigned char*)(d_point + 1);
	length = (unsigned short*)(d_point + 2);
	ipv4_addr_send = (unsigned int*)((unsigned char*)ipv4_addr_send + rcv_len);
	checksum_send += rcv_len;

	// Fill values
	*type_send = *type;
	*code_send = *code;
	*length_send = htons(rcv_len);
	strcpy(name_send, name);
	*ipv4_addr_send = htonl(ipv4_addr);
	*checksum_send = checksum_generate(buf_rcv, rcv_len);

	// Kernel response type check
	if (*type != MSG_REGISTER_RESPONSE) {
		printf("| NET_ERR | Kernel answer is untranslatable.\n");
		return MSG_KERNEL_NETERR;
	}

	// Kernel response code check
	if (*code != MSG_SUCCESS) {
		printf("| DUPLICA | Registration failed due to duplication.\n");
		return MSG_FAILED;
	}

	// Kernel response length check
	if (*length != ntohs(MSG_ADD_LENGTH)) {
		printf("| NET_ERR | Kernel answer is untranslatable.\n");
		return MSG_KERNEL_NETERR;
	}

	/* Termination */

	printf("| SUCCESS | Registration successful.\n");

	return MSG_SUCCESS;

}

unsigned short send_del_message(char* name, unsigned int ipv4_addr, char* buf_rcv) {
// buf_rcv : response message buffer for client, not netlink
// With this function, you can delete entry that has name and ipv4_addr.

	/* Declaration */

	char* d_point;

	unsigned char* type;
	unsigned char* code;
	unsigned short* length;

	unsigned char* type_send = (unsigned char*)buf_rcv;
	unsigned char* code_send = (unsigned char*)(buf_rcv + 1);
	unsigned short* length_send = (unsigned short*)(buf_rcv + 2);
	unsigned char* name_send = (unsigned char*)(buf_rcv + 4);
	unsigned int* ipv4_addr_send = (unsigned int*)(buf_rcv + 4);
	unsigned char* checksum_send = (unsigned char*)(buf_rcv + 8);

	unsigned short rcv_len = (unsigned short)strlen(name);
	struct nlmsghdr *nlh = NULL;
	char buf[NLMSG_SPACE(MAX_PAYLOAD)];

	/* Netlink packet exchange */

	memset(buf, 0, NLMSG_SPACE(MAX_PAYLOAD));

	// Send message
	nlh = (struct nlmsghdr*)buf;
	nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_flags = 0;

	printf("|  02/04  | Sending deregistration request(%s, %s) to kernel.\n", name, inet_ntoa(*(struct in_addr*)&ipv4_addr));
	set_del_message(name, ipv4_addr, (char*)NLMSG_DATA(nlh));
	send(sock_fd, buf, nlh->nlmsg_len, 0);
	printf("|  03/04  | Waiting for message from kernel.\n");

	// Receive message
	recv(sock_fd, buf, nlh->nlmsg_len, 0);
	d_point = (char*)NLMSG_DATA(nlh);

	/* Process message from kernel */

	// fix pointers
	type = (unsigned char*)d_point;
	code = (unsigned char*)(d_point + 1);
	length = (unsigned short*)(d_point + 2);
	ipv4_addr_send = (unsigned int*)((unsigned char*)ipv4_addr_send + rcv_len);
	checksum_send += rcv_len;

	// Fill Values
	*type_send = *type;
	*code_send = *code;
	*length_send = htons(rcv_len);
	strcpy(name_send, name);
	*ipv4_addr_send = htonl(ipv4_addr);
	*checksum_send = checksum_generate(buf_rcv, rcv_len);

	// Kernel response type check
	if (*type != MSG_DEREGISTER_RESPONSE) {
		printf("| NET_ERR | Kernel answer is untranslatable.\n");
		return MSG_KERNEL_NETERR;
	}

	// Kernel response code check
	if (*code != MSG_SUCCESS) {
		printf("| NOEXIST | Requested data does not exist.\n");
		return MSG_FAILED;
	}

	// Kernel response length check
	if (*length != ntohs(MSG_DEL_LENGTH)) {
		printf("| NET_ERR | Kernel answer is untranslatable.\n");
		return MSG_KERNEL_NETERR;
	}

	/* Termination */

	printf("| SUCCESS | Deregistration successful.\n");

	return MSG_SUCCESS;

}

unsigned short send_get_message(char* name, char* buf_rcv) {
// buf_rcv : response message buffer for client, not netlink
// With this function, you can get IPv4 address which of name is same.

	/* Declaration */

	char* d_point;

	unsigned char* type;
	unsigned char* code;
	unsigned short* length;
	unsigned int* ipv4_addr;

	unsigned char* type_send = (unsigned char*)buf_rcv;
	unsigned char* code_send = (unsigned char*)(buf_rcv + 1);
	unsigned short* length_send = (unsigned short*)(buf_rcv + 2);
	unsigned char* name_send = (unsigned char*)(buf_rcv + 4);
	unsigned int* ipv4_addr_send = (unsigned int*)(buf_rcv + 4);
	unsigned char* checksum_send = (unsigned char*)(buf_rcv + 8);

	unsigned short rcv_len = (unsigned short)strlen(name);
	struct nlmsghdr* nlh = NULL;
	char buf[NLMSG_SPACE(MAX_PAYLOAD)];

	/* Netlink packet exchange */

	memset(buf, 0, NLMSG_SPACE(MAX_PAYLOAD));

	// Send message
	nlh = (struct nlmsghdr*)buf;
	nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_flags = 0;

	printf("|  02/04  | Sending translation request(%s -> IPv4) to kernel.\n", name);
	set_get_message(name, (char*)NLMSG_DATA(nlh));
	send(sock_fd, buf, nlh->nlmsg_len, 0);
	printf("|  03/04  | Waiting for message from kernel.\n");

	// Receive message
	recv(sock_fd, buf, nlh->nlmsg_len, 0);
	d_point = (char*)NLMSG_DATA(nlh); // get data starting pointer to process received data
	
	/* Process message from kernel */

	// fix pointers
	type = (unsigned char*)d_point;
	code = (unsigned char*)(d_point + 1);
	length = (unsigned short*)(d_point + 2);
	ipv4_addr = (unsigned int*)(d_point + 4);
	ipv4_addr_send = (unsigned int*)((unsigned char*)ipv4_addr_send + rcv_len);
	checksum_send += rcv_len;

	// Fill values (1)
	*type_send = *type;
	*code_send = *code;
	*length_send = htons(rcv_len);
	strcpy(name_send, name);

	// Kernel response type check
	if (*type != MSG_GET_RESPONSE) {
		printf("| NET_ERR | Kernel answer is untranslatable.\n");
		return MSG_KERNEL_NETERR;
	}

	// Kernel response code check
	if (*code != MSG_SUCCESS) {
		printf("| NOEXIST | Requested data does not exist.\n");
		return MSG_FAILED;
	}

	// Kernel response length check
	if (*length != ntohs(MSG_GET_LENGTH)) {
		printf("| NET_ERR | Kernel answer is untranslatable.\n");
		return MSG_KERNEL_NETERR;
	}

	// Fill values (2)
	// Seperated because IPv4 address cannot exist when failure
	*ipv4_addr_send = htonl(*ipv4_addr);
	*checksum_send = checksum_generate(buf_rcv, rcv_len);

	/* Termination */

	printf("| SUCCESS | GET request successful.\n");

	return MSG_SUCCESS;

}

unsigned short set_add_message(char* name, unsigned int ipv4_addr, char* hdr_pointer) {
	
	// default kernel request header length 3 + sizeof(integer) + strlen(name) + NULL Value for string => 7 + strlen(name) + 1
	unsigned short msg_len = strlen(name) + 8;
	SET_MSG_CHAR(hdr_pointer, MSG_REGISTER)
	SET_MSG_SHORT(hdr_pointer, msg_len)
	SET_MSG_INTEGER(hdr_pointer, ipv4_addr)
	SET_MSG_STRING(hdr_pointer, name)

	return msg_len;

}

unsigned short set_del_message(char* name, unsigned int ipv4_addr, char* hdr_pointer) {
	
	// default kernel request header length 3 + sizeof(integer) + strlen(name) + NULL Value for string => 7 + strlen(name) + 1
	unsigned short msg_len = strlen(name) + 8;
	SET_MSG_CHAR(hdr_pointer, MSG_DEREGISTER)
	SET_MSG_SHORT(hdr_pointer, msg_len)
	SET_MSG_INTEGER(hdr_pointer, ipv4_addr)
	SET_MSG_STRING(hdr_pointer, name)

	return msg_len;

}

unsigned short set_get_message(char* name, char* hdr_pointer) {
	
	// default kernel request header length 3 + strlen(name) + NULL Value for string => 3 + strlen(name) + 1
	unsigned short msg_len = strlen(name) + 4;
	SET_MSG_CHAR(hdr_pointer, MSG_GET)
	SET_MSG_SHORT(hdr_pointer, msg_len)
	SET_MSG_STRING(hdr_pointer, name)
	
	return msg_len;

}

unsigned char checksum_generate(char* array, int name_length) {

	int i, length;
	unsigned char sum = 0;

	length = name_length + 9;

	for (i = 0; i < length - 1; i++) {
		sum += (unsigned char)array[i];
	}

	return sum;

}

bool checksum_verify(char* array, int name_length) {

	int i, length;
	unsigned char sum = 0;

	if (array[0] == MSG_GET) length = name_length + 4;
	else length = name_length + 8;

	for (i = 0; i < length - 1; i++) {
		sum += (unsigned char)array[i];
	}

	return (unsigned char)array[length - 1] == sum;

}
