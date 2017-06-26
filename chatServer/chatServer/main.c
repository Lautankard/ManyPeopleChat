//
//  main.c
//  chatServer
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
#include <sys/select.h>

#define SERVER_HOST "127.0.0.1"
#define SERVER_PORT 8001
#define MAX_CONNECT 128
#define BUFFER_SIZE 2048
typedef void *(*pthread_call)();

struct chatItem {
    int sockfd;
    char ip[32];
    char name[16];
};

void error_parse(char *str) {
    printf("%s\n", str);
    exit(0);
}

int user_connect = 0;
int chat_sock_list[MAX_CONNECT] = {0};
fd_set sendfd, recvfd;
int maxfd = 0;

int initServerSock(int type, struct sockaddr* addr, int addr_len, int listen_size);
int create_thread_detach(int *thread_id, pthread_call func, void *argv);
void multipleThreadServer(int server_sockfd, struct sockaddr_in *addr, int* addr_len);
void selectIOServer(int server_sockfd, struct sockaddr_in *addr, int* addr_len);
void *chat_login(void *argv);
void *chat_doing(void *argv);
void chat_send(const char *msg, int from_sockfd);
void chat_exit(struct chatItem *p);
struct chatItem *createChatItem(int connect_sockfd, struct sockaddr_in* addr);
int select_login(struct chatItem **lp, struct chatItem *p);
void select_chat_exit(struct chatItem **lp, struct chatItem *p);

void printItem(struct chatItem *p) {
    printf("sockfd: %d, ip: %s, name: %s\n",p->sockfd, p->ip, p->name);
}

int main(int argc, const char * argv[]) {
    int ser_sockfd, cli_addr_len;
    struct sockaddr_in server_sockaddr, cli_sockaddr;
    
    server_sockaddr.sin_family = AF_INET;
    server_sockaddr.sin_addr.s_addr = inet_addr(SERVER_HOST);
    server_sockaddr.sin_port = htons(SERVER_PORT);
    
    ser_sockfd = initServerSock(SOCK_STREAM, (struct sockaddr*) &server_sockaddr, sizeof(server_sockaddr), MAX_CONNECT);
    if (ser_sockfd < 0) {
        error_parse("create server socket fail");
    }
    printf("---create success---\n");
    
//    multipleThreadServer(ser_sockfd, &cli_sockaddr, &cli_addr_len);
    selectIOServer(ser_sockfd, &cli_sockaddr, &cli_addr_len);
    printf("主线程退出\n");
    pthread_exit(NULL);
    return 1;
}

void selectIOServer(int server_sockfd, struct sockaddr_in *addr, int* addr_len) {
    int connect_sockfd = -1;
    maxfd = server_sockfd;
    char buffer[BUFFER_SIZE] = {0};
    struct chatItem *chat_item_list[MAX_CONNECT] = {0};
    
    FD_ZERO(&sendfd);
    FD_ZERO(&recvfd);
    FD_SET(server_sockfd, &recvfd);
    
    while(1) {
        printf("maxfd %d\n", maxfd);
        fd_set rset = recvfd;
        int ready_count = select(maxfd + 1, &rset, NULL, NULL, NULL);
        printf("ready_count %d\n",ready_count);
        if (ready_count > 0) {
            if (FD_ISSET(server_sockfd, &rset)) { //有连接过来
                connect_sockfd = accept(server_sockfd, (struct sockaddr *)addr, addr_len);
                if (connect_sockfd >= 0) {
                    struct chatItem *item = createChatItem(connect_sockfd, addr);
                    chat_sock_list[user_connect] = connect_sockfd;
                    chat_item_list[user_connect] = item;
                    user_connect++;
                    FD_SET(connect_sockfd, &recvfd);
                    if (maxfd < connect_sockfd) maxfd = connect_sockfd;
                }
            }
            
            for (int i = 0; i < user_connect; ++i) {
                if (FD_ISSET(chat_sock_list[i], &rset)) {
                    memset(buffer, 0, BUFFER_SIZE);
                    if (connect_sockfd == chat_sock_list[i]) { //发送的登录信息
                        connect_sockfd = -1;
                        if (select_login(chat_item_list, chat_item_list[i]) < 0) {
                            --i;
                        }
                    } else { //聊天信息
                        int recv_len = recv(chat_sock_list[i], buffer, BUFFER_SIZE, 0);
                        printf("recv_len %d\n", recv_len);
                        if (recv_len <=0 || strcmp(buffer, "exit\n") == 0) {
                            select_chat_exit(chat_item_list, chat_item_list[i]);
                            i--;
                            continue;
                        }
                        chat_send(buffer, chat_sock_list[i]);
                    }
                }
            }
        }
    }
}

int select_login(struct chatItem **lp, struct chatItem *p) {
    printf("select 登陆成功\n");
    char buffer[BUFFER_SIZE] = {0};
    printItem(p);
    int recv_len = recv(p->sockfd, buffer, BUFFER_SIZE, 0);
    if (recv_len <=0 || strcmp(buffer, "exit\n") == 0) {
        select_chat_exit(lp, p);
        return -1;
    } else {
        strcpy(p->name, buffer);
        memset(buffer, 0, BUFFER_SIZE);
        sprintf(buffer, "%s 进入聊天室", p->name);
        printf("%s\n", buffer);
        chat_send(buffer, 0);
    }
    return 0;
}
int getMaxFd(fd_set *fd_pointer, int oldMaxFd) {
    int maxFd = 0;
    for (int i = 0; i <= oldMaxFd; ++i) {
        if (FD_ISSET(i, fd_pointer) && maxFd < i) {
            maxFd = i;
        }
    }
    return maxFd;
}

void select_chat_exit(struct chatItem **lp, struct chatItem *p) {
    if (!p) return;
    int i = 0;
    for (; i < user_connect; ++i) {
        if (lp[i] == p) {
            FD_CLR(p->sockfd, &recvfd);
            maxfd = getMaxFd(&recvfd, maxfd);
            close(p->sockfd);
            free(p);
            lp[i] = NULL;
            break;
        }
    }
    if (i < user_connect) {
        memmove(chat_sock_list, chat_sock_list + i + 1, MAX_CONNECT - i - 1);
        memmove(lp, lp + i + 1, MAX_CONNECT - i - 1);
        user_connect--;
    }
}

//多线程服务器
void multipleThreadServer(int server_sockfd, struct sockaddr_in *addr, int* addr_len) {
    int connect_sockfd, chat_user_count = 0;;
    pthread_t th1id;
    struct sockaddr_in6* p;
    while(1) {
        connect_sockfd = accept(server_sockfd, (struct sockaddr *)addr, addr_len);  //阻塞
        if (connect_sockfd >= 0) {
            struct chatItem *item = createChatItem(connect_sockfd, addr);
            sprintf(item->name, "guest_%d", ++chat_user_count);
            chat_sock_list[user_connect++] = connect_sockfd;
            printItem(item);
            if (create_thread_detach(&th1id, chat_login, (void *)item)) {
                printf("线程创建失败\n");
            }
        }
    }
}

struct chatItem *createChatItem(int connect_sockfd, struct sockaddr_in* addr) {
    struct chatItem *item = (struct chatItem*) malloc(sizeof(struct chatItem));
    item->sockfd = connect_sockfd;
    strcpy(item->ip, inet_ntoa(addr->sin_addr));
    return item;
}

int create_thread_detach(int *thread_id, pthread_call func, void *argv) {
    int errno;
    pthread_attr_t pt;
    pthread_attr_init(&pt);
    pthread_attr_setdetachstate(&pt, PTHREAD_CREATE_DETACHED);
    if ((errno = pthread_create(thread_id, &pt, func, argv))) {
        printf("线程创建失败\n");
        return errno;
    }
    pthread_attr_destroy(&pt);
    return 0;
}

int initServerSock(int type, struct sockaddr* addr, int addr_len, int listen_size) {
    int ser_sockfd, sock_opt = 1;
    if ((ser_sockfd = socket(addr->sa_family, type, 0)) < 0) {
        return -1;
    }
    setsockopt(ser_sockfd, SOL_SOCKET, SO_REUSEADDR, &sock_opt, sizeof(sock_opt));//可以复用绑定，否则再次绑定会失败
    if (bind(ser_sockfd, addr, addr_len) < 0) {
        goto errorout;
    }
    if (type == SOCK_STREAM || type == SOCK_SEQPACKET) {
        if (listen(ser_sockfd, listen_size) < 0) {
            goto errorout;
        }
    }
    
    return ser_sockfd;
errorout:
    close(ser_sockfd);
    return -1;
}

void *chat_login(void *argv) {
    printf("登录请求");
    int th1id;
    struct chatItem *chat_pointer = (struct chatItem*) argv;
    char buffer[BUFFER_SIZE] = {0};
    memset(buffer, 0, BUFFER_SIZE);
    while(1) {
        memset(buffer, 0, BUFFER_SIZE);
        int recv_len = recv(chat_pointer->sockfd, buffer, BUFFER_SIZE, 0);
        if (recv_len <= 0 || strcmp(buffer, "exit\n") == 0) {
            send(chat_pointer->sockfd, "登录失败", strlen("登录失败"), 0);
            chat_exit(chat_pointer);
            break;
        } else {
            strcpy(chat_pointer->name, buffer);
            if (create_thread_detach(&th1id, chat_doing, chat_pointer)) {
                send(chat_pointer->sockfd, "登录失败, 请重新登陆", strlen("登录失败，请重新登陆"), 0);
            } else {
                break;
            }
        }
    }
    return NULL;
}

void *chat_doing(void *argv) {
    printf("登录成功");
    struct chatItem *chat_pointer = (struct chatItem*) argv;
    char buffer[BUFFER_SIZE] = {0};
    sprintf(buffer, "%s 进入聊天室", chat_pointer->name);
    printf("%s\n", buffer);
    chat_send(buffer, 0);
    memset(buffer, 0, BUFFER_SIZE);
    while(1) {
        memset(buffer, 0, BUFFER_SIZE);
        int recv_len = recv(chat_pointer->sockfd, buffer, BUFFER_SIZE, 0);
        if (recv_len <= 0 || strcmp(buffer, "exit\n") == 0) {
            chat_exit(chat_pointer);
            break;
        }
        chat_send(buffer, chat_pointer->sockfd);
    }
    return NULL;
}

void chat_send(const char *msg, int from_sockfd) {
    printf("发送: %s\n", msg);
    for (int i = 0; i < user_connect; ++i) {
        if (from_sockfd != 0 && chat_sock_list[i] == from_sockfd) continue;
        if (chat_sock_list[i] == 0) continue;
        send(chat_sock_list[i], msg, strlen(msg), 0);
    }
}

void chat_exit(struct chatItem *p) {
    char buffer[BUFFER_SIZE] = {0};
    sprintf(buffer, "%s 退出聊天室", p->name);
    chat_send(buffer, p->sockfd);
    printf("user logout, ip: %s, name: %s\n", p->ip, p->name);
    close(p->sockfd);
    int i = 0;
    for (; i < user_connect; ++i) {
        if (chat_sock_list[i] == p->sockfd) {
            free(p);
            break;
        }
    }
    if (i < user_connect) {
        memmove(chat_sock_list, chat_sock_list + i + 1, MAX_CONNECT - i - 1);
        user_connect--;
    }
}
