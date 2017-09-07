#ifndef _HTTP_SERVER_H_
#define _HTTP_SERVER_H_

#include <sys/socket.h>
#include <sys/types.h>
#include <strings.h> // bzero
#include <string.h> //read_conf
#include <sys/epoll.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h> //struct hostent*gethostbyname(const char *name);
#include <errno.h>
#include <fcntl.h>
#include <unistd.h> // close
#include <sys/sendfile.h>
#include <pthread.h>


#define MAX_EVENT_NUMBER 1024//epoll最大监听事件数
#define MAX_BACKLOG 100 //监听队列最大数

#define ONEKILO 1024
#define ONEMEGA 1024*1024
#define ONEGIGA 1024*1024*1024

#define CONF_BUFFERLEN 100
#define DELIM "=" 

typedef struct
{
  void *root;
  int port;
}
conf_t;

typedef struct
{
  conf_t cf;
  int con_fd;
} 
conf_con_fd_t;



#endif
