#ifndef _DO_HTTP_
#define _DO_HTTP_
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <time.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include "list.h"

typedef struct 
{
  const char *type;
  const char *value;
}
mime_type_t;


typedef struct 
{
  void *root;
  int fd;

  char *buff;
  size_t pos, last;
  int state;

  void *request_start;

  void *method_end;
  char *method;

  void *uri_start;
  void *uri_end;
  char *uri;
  
  void *path_start;
  void *path_end;

  int http_major;
  int http_minor;

  void *request_end;

  void *cur_header_key_start;
  void *cur_header_key_end;

  list_head list;
  void *cur_header_value_start;
  void *cur_header_value_end;
}
http_request_t;


typedef struct
{
  void *key_start, *key_end;
  void *value_start, *value_end;
  list_head list;
}
http_header_t;

typedef struct
{
  int fd;
  int keep_alive;
  time_t mtime;
  int modified;
  int status;
}
http_response_t;

void do_http_request(int con_fd,char *root,char *buff, size_t buff_size);

#endif
