//
//  main.c
//  chatClient
//
//  Created by 陈鹰 on 17/5/18.
//  Copyright © 2017年 chen ying. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define SERVER_HOST "127.0.0.1"
#define SERVER_PORT 8001
#define MAXSLEEP 32
#define BUFFER_SIZE 2048

char word[BUFFER_SIZE];

void error_parse(char *str) {
    printf("%s\n", str);
    exit(0);
}
//每次尝试重连都要打开新的套接字， 因为旧的连接失败的话，继续使用照旧会失败，历史遗留问题
int connet_retry(int domain, int type, int protocol, struct sockaddr* addr, int addr_len) {
    int numsec, sockfd;
    for (numsec = 1; numsec < MAXSLEEP; numsec <<= 1) {
        if ((sockfd = socket(domain, type, protocol)) < 0) {
            return -1;
        }
        
        if (connect(sockfd, addr, addr_len) == 0) {
            return sockfd;
        }
        printf("wait %d sec\n", numsec);
        close(sockfd);
        if (numsec <= MAXSLEEP / 2) sleep(numsec);
    }
    return -1;
}

void *show_msg(void *argv);
void *send_msg(void *argv);
const char *user_name = "大熊";

int main(int argc, const char * argv[]) {
    if (argc > 1) user_name = argv[1];
    int cli_sockfd;
    pthread_t th1id, th2id;
    struct sockaddr_in server_sockaddr;
    server_sockaddr.sin_family = AF_INET;
    server_sockaddr.sin_addr.s_addr = inet_addr(SERVER_HOST);
    server_sockaddr.sin_port = htons(SERVER_PORT);
    cli_sockfd = connet_retry(AF_INET, SOCK_STREAM, 0, (struct sockaddr*) &server_sockaddr, sizeof(server_sockaddr));
    if (cli_sockfd == -1) {
        error_parse("登录失败，请重试");
    }
    printf("登录成功\n");
    pthread_attr_t pthreadattr;
    pthread_attr_init(&pthreadattr);
    pthread_attr_setdetachstate(&pthreadattr, PTHREAD_CREATE_DETACHED);
    pthread_create(&th1id, &pthreadattr, send_msg, &cli_sockfd);
    pthread_create(&th2id, &pthreadattr, show_msg, &cli_sockfd);
    pthread_attr_destroy(&pthreadattr);
    pthread_exit((void *)0);
    return 1;
}

void *show_msg(void *argv) {
    int sockfd = *(int *)argv;
    char buffer[BUFFER_SIZE] = {0};
    while(1) {
        int write_len = recv(sockfd, buffer, BUFFER_SIZE, 0);
        printf("write_len %d\n",write_len);
        if (write_len <= 0) break;
//        if (recv(sockfd, buffer, BUFFER_SIZE, 0) <= 0) break;
        fputs(buffer, stdout);   //fputs是行缓冲
        fputc('\n', stdout);
        memset(buffer, 0, BUFFER_SIZE);
    }
    printf("show_msg\n");
    exit(0);
}

void *send_msg(void *argv) {
    int sockfd = *(int *)argv;
    int read_len;
    char buffer[BUFFER_SIZE] = {0};
    char chat_msg[2 * BUFFER_SIZE] = {0};
    send(sockfd, user_name, strlen(user_name), 0);
    while(fgets(buffer, BUFFER_SIZE, stdin)) {
        if (strcmp(buffer, "\n") == 0) continue;
        fputc('\n', stdout);
        strcat(chat_msg, user_name);
        strcat(chat_msg, ":  ");
        strcat(chat_msg, buffer);
        send(sockfd, chat_msg, strlen(chat_msg), 0);
        memset(buffer, 0, BUFFER_SIZE);
        memset(chat_msg, 0, sizeof(chat_msg));
    }
    printf("send_msg\n");
    return NULL;
}
