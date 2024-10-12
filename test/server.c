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
#include <signal.h>
#include <time.h>


#include "test.h"
#include "up3d.h"

#define SERVER_IP   "127.0.0.1"
#define SERVER_PORT 8888

#define KCP_CONV 12345

struct sockaddr_in server_addr, client_addr;
ikcpcb *kcp;

int udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
    static int cnt = 0;
	int sockfd = *(int *)user;
    struct sockaddr_in server_addr;
    int client_len = sizeof(struct sockaddr_in);
    
    _LOG(_LOG_DEBUG, "udp_output len:%d cnt:%d\n", len, cnt++);
    return sendto(sockfd, buf, len, 0, (struct sockaddr*)&client_addr, client_len);
}

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

/*
    服务器端接收数据
*/
int main()
{
    int sockfd, len, client_len;
    uint32_t conv = KCP_CONV; // 这里可以设置协议的会话编号
    char buf[4096] = {0};
    int n, ret;
    int pyload_cnt = 0, kcp_raw_cnt = 0, pyload_tol_cnt = 0, index = 0;
    client_len = sizeof(client_addr);
    
    _LOG(_LOG_INFO, "test server\n");
    // 创建 UDP 套接字并绑定到指定端口
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket error");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_PORT);

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind error");
        exit(EXIT_FAILURE);
    }

    // 初始化 KCP 协议控制块
    kcp = ikcp_create(conv, (void *)&sockfd);
    kcp->output = udp_output;

    // 将 KCP 协议控制块设为快速模式，以提高传输速度
    ikcp_nodelay(kcp, 1, 10, 2, 1);

    while (1) 
    {
        usleep(10*100);
        ikcp_update(kcp, iclock()); // 需要回复ACK 所以这里要定时调一下这个ikcp_update接口
        len = recvfrom(sockfd, buf, sizeof(buf), MSG_DONTWAIT, (struct sockaddr*)&client_addr, &client_len);
        if (len <= 0) {
            continue;
        } else {
            _LOG(_LOG_DEBUG, "len:%d index:%02d\n", len, index++);
            kcp_raw_cnt += len;
            ret = ikcp_input(kcp, buf, len);
            while(1)
            {
                n = ikcp_recv(kcp, buf, sizeof(buf));
                if (n < 0) {
                    _LOG(_LOG_INFO, "n:%d ret:%d kcp_raw_cnt:%5d pyload_cnt:%4d pyload_tol_cnt:%6d\n", n, ret, kcp_raw_cnt, pyload_cnt, pyload_tol_cnt);
                    pyload_cnt = 0;
                    kcp_raw_cnt = 0;
                    break;
                } else {
                    pyload_cnt += n;
                    pyload_tol_cnt += n;
                }
            }
            
        }
    }

    ikcp_release(kcp);
    close(sockfd);

    return 0;
}
