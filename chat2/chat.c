#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <netinet/in.h>

#define MAX_MESSAGE_LEN 100
#define MAX_IP_LEN 16

int main() {
    char message[MAX_MESSAGE_LEN];


    printf("Enter a message: ");
    fgets(message, MAX_MESSAGE_LEN, stdin);
    message[strcspn(message, "\n")] = '\0';  // 개행 문자 제거

 

    long result = syscall(451, message);
    if (result == 0) {
        printf("sys_server() syscall succeeded\n");
    } else {
        printf("sys_server() syscall failed with error code %ld\n", result);
        return -1;
    }
    return 0;
}
