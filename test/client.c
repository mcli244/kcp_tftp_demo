#include "ikcp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "test.h"

#define SERVER_IP   "127.0.0.1"
#define SERVER_PORT 8888
#define CLIENT_PORT 8001

#define KCP_CONV 12345
struct sockaddr_in server_addr;
ikcpcb *kcp;

int make_socket_non_blocking(int sockfd)
{
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        return -1;
    }
    flags |= O_NONBLOCK;
    if (fcntl(sockfd, F_SETFL, flags) == -1) {
        return -1;
    }
    return 0;
}

int udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
    static int cnt = 0;
    static int index = 0;
	int sockfd = *(int *)user;
    int client_len = sizeof(struct sockaddr_in);

    cnt += len;
    printf("%02d: udp_output len:%d cnt:%d\n", index++, len, cnt);
    return sendto(sockfd, buf, len, 0, (struct sockaddr*)&server_addr, client_len);
}

// 100ms
void timer_handler(int signum) {
    // printf("timer_handler\n");
    ikcp_update(kcp, iclock());
}


int init_timer(void)
{
    struct sigaction sa;
    struct itimerspec timer_spec;
    timer_t timerid;

    // 设置定时器信号处理函数
    sa.sa_flags = SA_SIGINFO;
    sa.sa_handler = timer_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGALRM, &sa, NULL);

    // 创建POSIX定时器
    if (timer_create(CLOCK_REALTIME, NULL, &timerid) == -1) {
        perror("timer_create");
        return -1;
    }

    // 定时器的初始时间和间隔时间设置
    timer_spec.it_value.tv_sec = 0;  // 初始延时1秒
    timer_spec.it_value.tv_nsec = 100*1000*1000;
    timer_spec.it_interval.tv_sec = 0;  // 间隔时间2秒
    timer_spec.it_interval.tv_nsec = 100*1000*1000;

    // 设置定时器
    if (timer_settime(timerid, 0, &timer_spec, NULL) == -1) {
        perror("timer_settime");
        return -1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    
    int sockfd, len, maxfd, nready;
    uint32_t conv = KCP_CONV; // 这里需要和服务端保持一致
    fd_set rset, allset;
    

    printf("test client\n");

    if(argc != 4)
    {
        printf("Usage: %s ip port filename\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // 创建 UDP 套接字
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket error");
        exit(EXIT_FAILURE);
    }

    printf("server:%s:%d\n", argv[1], atoi(argv[2]));
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);
    server_addr.sin_port = htons(atoi(argv[2]));

    // memset(&local_addr, 0, sizeof(local_addr));
    // local_addr.sin_family = AF_INET;
    // local_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    // local_addr.sin_port = htons(CLIENT_PORT);
    // if (bind(sockfd, (struct sockaddr*)&local_addr, sizeof(server_addr)) < 0) {
    //     perror("bind error");
    //     exit(EXIT_FAILURE);
    // }

    // if (make_socket_non_blocking(sockfd) == -1) {
    //     perror("make_socket_non_blocking error");
    //     exit(EXIT_FAILURE);
    // }

    // 初始化 KCP 协议控制块
    kcp = ikcp_create(conv, (void *)&sockfd);
    kcp->output = udp_output;
    // 将 KCP 协议控制块设为快速模式，以提高传输速度
    ikcp_nodelay(kcp, 1, 10, 2, 1);
    
    ikcp_update(kcp, 0);
    // init_timer();

    uint8_t buf[1025];
    int n = 0;
    int pyload_cnt = 0;

    
    int kcp_raw_cnt = 0;
    int ret = 0;
    int send_cnt = 0;

    FILE *file;
    if ((file = fopen(argv[3], "r")) == NULL)
        goto err;

    char msg[256];

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(STDIN_FILENO, F_SETFL, flags);
    while(1)
    {
        if(fgets(msg , sizeof(msg) , stdin) != NULL)
        {
            printf("msg:[%s]\n", msg);
            if(strcmp(msg , "send\n") == 0)
            {
                fseek(file, 0, SEEK_SET);
                pyload_cnt = 0;
                send_cnt = 0;
                while (1) {
                    n = fread(buf, 1, sizeof(buf), file);
                    if(n <= 0)
                        break;
                    ret = ikcp_send(kcp, buf, n);
                    // printf("ikcp_send ret:%d\n", ret);
                    ikcp_flush(kcp);
                    pyload_cnt += n;
                    send_cnt ++;
                }
                printf("n:%d ret:%d send_cnt:%d pyload_cnt:%d \n", n, ret, send_cnt, pyload_cnt);
            }
            else if(strcmp(msg , "q\n") == 0)
            {
                break;
            }
        }
        ikcp_update(kcp, 0);
        usleep(100*1000);
    }
    
    fclose(file);

err:
    ikcp_release(kcp);
    close(sockfd);
    printf("exit\n");
    return 0;
}
