#ifndef __UP3D_H__
#define __UP3D_H__

#include <stdio.h>
#include <time.h>
#include <sys/time.h>

//定义日志级别
typedef enum  {    
    _LOG_OFF=0,
    _LOG_FATAL,
    _LOG_ERR,
    _LOG_WARN,
    _LOG_INFO,
    _LOG_DEBUG,
    _LOG_ALL
}LOG_LEVEL;

#define UP_LOG_ENABLE   /* 日志模块总开关，注释掉将关闭日志输出 */
#define UP_LOG_LEVEL    _LOG_INFO /* 日志输出控制，小于等于UP_LOG_LEVEL等级的日志被输出 */
#define NEWLINE_SIGN    "\n"

FILE *log_fd;
int log_tmp_fd;
/* 日志时间戳,精确到毫秒 */
char* get_stime(void)
{
    static char timestr[200] ={0};
    struct tm * pTempTm;
    struct timeval time;

    gettimeofday(&time,NULL);
    pTempTm = localtime(&time.tv_sec);
    if( NULL != pTempTm )
    {
        snprintf(timestr,199,"%04d-%02d-%02d %02d:%02d:%02d.%03ld",
            pTempTm->tm_year+1900,
            pTempTm->tm_mon+1,
            pTempTm->tm_mday,
            pTempTm->tm_hour,
            pTempTm->tm_min,
            pTempTm->tm_sec,
            time.tv_usec/1000);
    }
    return timestr;
}

#ifdef UP_LOG_ENABLE

#define DEBUG(format, ...) \
    do { \
        printf (format, ##__VA_ARGS__); \
        if(log_fd) \
        { \
            fprintf(log_fd, format, ##__VA_ARGS__);\
            fflush(log_fd); \
        } \
    } while (0)
#else
    #define DEBUG(format, ...)
#endif

#define _LOG_INIT(logFile)\
    do { \
        log_fd = fopen (logFile, "w+"); \
        if (log_fd == NULL) \
        { \
            printf("log file [%s] open failed\n", logFile); \
        } \
    } while (0)

#define _LOG_CLOSE()\
    do { \
        fclose(log_fd);\
        log_fd = NULL; \
    } while (0)

#define _LOG_CLEAN()\
    do { \
        log_tmp_fd = fileno(log_fd); \
        ftruncate(log_tmp_fd, 0); \
    } while (0)

#define _LOG(level, format, ...)\
    do { \
         if(level<=UP_LOG_LEVEL){\
            if(level==_LOG_DEBUG){\
                DEBUG("D|[%s] %s:%d(%s)|"format"%s", get_stime(), __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__, NEWLINE_SIGN);\
            }else if(level==_LOG_INFO){\
                DEBUG("I|[%s] "format"%s", get_stime(), ##__VA_ARGS__, NEWLINE_SIGN);\
            }else if(level==_LOG_WARN){\
                DEBUG("W|[%s] "format"%s", get_stime(), ##__VA_ARGS__, NEWLINE_SIGN);\
            }else if(level==_LOG_ERR){\
                DEBUG("E|[%s] %s:%d(%s)|"format"%s", get_stime(), __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__, NEWLINE_SIGN);\
            }else if(level==_LOG_FATAL){\
                DEBUG("F|[%s] %s:%d(%s)|"format"%s", get_stime(), __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__, NEWLINE_SIGN);\
            }\
         } \
    } while (0)

#endif