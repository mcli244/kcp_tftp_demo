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
#include <pthread.h>

#include "test.h"
#include "up3d.h"

#define SERVER_IP   "127.0.0.1"
#define SERVER_PORT 8888
#define CLIENT_PORT 8001

#define KCP_CONV 12345


typedef struct
{
    int sfd;
    int sys_runing;
    struct sockaddr_in server_addr;
    pthread_t recv_pid;
    pthread_t send_pid;
    ikcpcb *kcp;
    char *file_name;
}UP3D_Client_Ctx_Type;

UP3D_Client_Ctx_Type cctx;


int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        return -1;
    }
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1) {
        return -1;
    }
    return 0;
}

int udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
    static int cnt = 0;
    static int index = 0;
    int slen = sizeof(struct sockaddr_in);
    UP3D_Client_Ctx_Type *cctx = (UP3D_Client_Ctx_Type *)user;

    cnt += len;
    _LOG(_LOG_DEBUG, "%02d: udp_output len:%d cnt:%d\n", index++, len, cnt);
    return sendto(cctx->sfd, buf, len, 0, (struct sockaddr*)&cctx->server_addr, slen);
}

int socket_creat(char *s_ip, int s_port)
{
    int sockfd;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket error");
        exit(EXIT_FAILURE);
    }

    printf("server:%s:%d\n", s_ip, s_port);
    memset(&cctx.server_addr, 0, sizeof(cctx.server_addr));
    cctx.server_addr.sin_family = AF_INET;
    cctx.server_addr.sin_addr.s_addr = inet_addr(s_ip);
    cctx.server_addr.sin_port = htons(s_port);

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

    return sockfd;
}

void *recv_process(void *arg)
{
    int len, remote_len, ret, n;
    uint8_t buf[1024];
    struct sockaddr_in remote_addr;
    
    UP3D_Client_Ctx_Type *cctx = (UP3D_Client_Ctx_Type *)arg;
    remote_len = sizeof(struct sockaddr_in);
    
    _LOG(_LOG_INFO, "recv_process start");
    
    while(cctx->sys_runing)
    {
        ikcp_update(cctx->kcp, iclock());
        usleep(10*1000);
        len = recvfrom(cctx->sfd, buf, sizeof(buf), MSG_DONTWAIT, (struct sockaddr*)&remote_addr, &remote_len);
        if (len <= 0) {
            continue;
        } else {
            _LOG(_LOG_DEBUG, "len:%d", len);
            ret = ikcp_input(cctx->kcp, buf, len);
            if(ret < 0) continue;// 有可能是控制报文，或者ACK等
            while(1)    // 真正数据
            {
                n = ikcp_recv(cctx->kcp, buf, sizeof(buf));
                if (n < 0) {
                    break;
                } else {
                    _LOG(_LOG_DEBUG, "received data from client: %d bytes ret:%d \n", n, ret);
                }
            }
        }
    }

    _LOG(_LOG_INFO, "recv_process exit");
}

void *send_process(void *arg)
{
    int len, ret, n;
    uint8_t buf[1024];
    char msg[256];
    int pyload_cnt = 0, kcp_raw_cnt = 0, send_cnt = 0;
    UP3D_Client_Ctx_Type *cctx = (UP3D_Client_Ctx_Type *)arg;


    FILE *file;
    if ((file = fopen(cctx->file_name, "r")) == NULL)
    {
        _LOG(_LOG_ERR, "fopen failed %s", cctx->file_name);
        return NULL;
    }

    set_nonblocking(STDIN_FILENO);

    _LOG(_LOG_INFO, "send_process start");
    while(cctx->sys_runing)
    {
        if(fgets(msg , sizeof(msg) , stdin) != NULL)
        {
            _LOG(_LOG_INFO, "msg:[%s]\n", msg);
            if(strcmp(msg , "send\n") == 0)
            {
                fseek(file, 0, SEEK_SET);
                pyload_cnt = 0;
                send_cnt = 0;
                while (1) {
                    n = fread(buf, 1, sizeof(buf), file);
                    if(n <= 0)  break;
                    ret = ikcp_send(cctx->kcp, buf, n);
                    pyload_cnt += n;
                    send_cnt ++;
                }
                _LOG(_LOG_INFO, "n:%d ret:%d send_cnt:%d pyload_cnt:%d \n", n, ret, send_cnt, pyload_cnt);
            }
            else if(strcmp(msg , "q\n") == 0)
            {
                cctx->sys_runing = 0;
                break;
            }
        }
        usleep(100*1000);
    }

    fclose(file);
    _LOG(_LOG_INFO, "send_process exit");
}

int main(int argc, char **argv)
{
    
    int ret, len;
    uint32_t conv = KCP_CONV; // 这里需要和服务端保持一致

    _LOG(_LOG_INFO, "test client\n");
    if(argc != 4)
    {
        _LOG(_LOG_ERR, "Usage: %s ip port filename\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    cctx.sys_runing = 1;
    cctx.file_name = argv[3];
    cctx.sfd = socket_creat(argv[1], atoi(argv[2]));
    
    cctx.kcp = ikcp_create(conv, (void *)&cctx);
    cctx.kcp->output = udp_output;
    ikcp_nodelay(cctx.kcp, 1, 10, 2, 1);
    ikcp_update(cctx.kcp, 0);   // 必须要调这个
    
    ret = pthread_create(&cctx.recv_pid , NULL , recv_process, (void *)&cctx);
    if(0 != ret)
    {
        _LOG(_LOG_ERR, "pthread_create failed");
        perror("pthread_create");
        goto err;
    }

    ret = pthread_create(&cctx.send_pid , NULL , send_process, (void *)&cctx);
    if(0 != ret)
    {
        _LOG(_LOG_ERR, "pthread_create failed");
        perror("pthread_create");
        goto err;
    }

    pthread_detach(cctx.recv_pid);
    pthread_detach(cctx.send_pid);

    while(cctx.sys_runing)
    {
        sleep(3);
    }
    
err:
    ikcp_release(cctx.kcp);
    close(cctx.sfd);
    _LOG(_LOG_INFO, "exit\n");
    return 0;
}
