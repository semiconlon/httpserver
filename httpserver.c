#include "httpserver.h"
#include "do_http.h"

int read_conf(char *filename, conf_t *ptr_cf, char *buf, int len );
void* deal_connect(void *para );

int thread_num = 0;
pthread_mutex_t thread_num_mutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char *argv[])
{

  int lis_fd;
  int con_fd;
  int nfds;
  int epoll_fd;
  int lis_fd_flags;
  int con_fd_flags;

  struct epoll_event event;
  struct epoll_event event_set[MAX_EVENT_NUMBER]; 

  struct sockaddr_in cli_addr;
  struct sockaddr_in serv_addr;
  socklen_t addr_len;

  pthread_attr_t pthread_attr;
  pthread_t tid;

  conf_con_fd_t conf_con_fd;
  conf_t cf;

   
  if(argc != 2)
  {
    printf("Usage: %s <config_path>\n", argv[0]);
    exit(-1);
  }

  char conf_buf[CONF_BUFFERLEN];
  int rc = read_conf(argv[1], &cf, conf_buf, CONF_BUFFERLEN);
  if(rc != 0)
  {
    printf("Can't open configure file\n");
    exit(-1);
  }

  printf("The listenning port is %d\n",cf.port);
  printf("The root path is %s\n",(char *)cf.root);
  conf_con_fd.cf = cf;

  lis_fd = socket(AF_INET, SOCK_STREAM, 0);

  lis_fd_flags = fcntl(lis_fd, F_GETFL);
  lis_fd_flags |= O_NONBLOCK;
  fcntl(lis_fd_flags, F_SETFL, lis_fd_flags);

  setsockopt(lis_fd, SOL_SOCKET, SO_REUSEADDR, NULL, 0);

  bzero(&serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons((unsigned short)cf.port);
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  bind(lis_fd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
  listen(lis_fd, MAX_BACKLOG);

  epoll_fd =  epoll_create(MAX_EVENT_NUMBER);

  event.events = EPOLLIN;
  event.data.fd = lis_fd;

  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, lis_fd, &event);

  pthread_attr_init(&pthread_attr);
  pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_DETACHED);

  for(;;)
  {
    nfds = epoll_wait(epoll_fd, event_set, MAX_EVENT_NUMBER, -1);

    if(nfds == -1 && errno == EINTR)
      continue;

    for(int n = 0; n!= nfds; ++n)
    {

      if(event_set[n].data.fd == lis_fd)
      {
        con_fd = accept(lis_fd, (struct sockaddr*)&cli_addr, &addr_len);

        con_fd_flags = fcntl(con_fd, F_GETFL);
        con_fd_flags |= O_NONBLOCK;
        fcntl(con_fd, F_SETFL, con_fd_flags);

        event.events = EPOLLIN | EPOLLET;
        event.data.fd = con_fd;

        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, con_fd, &event);
      }
      else
      {
        conf_con_fd.con_fd = event_set[n].data.fd;

        event.data.fd = con_fd;
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, con_fd, &event);

        pthread_create(&tid, &pthread_attr, &deal_connect, (void *) &conf_con_fd);
      }
    }

    pthread_attr_destroy(&pthread_attr);

  }
  close(lis_fd);
  return 0; 

}


void* deal_connect(void *para)
{
  pthread_mutex_lock(&thread_num_mutex);
  ++thread_num;
  pthread_mutex_unlock(&thread_num_mutex);
 
  conf_con_fd_t *ptr_conf_con_fd = (conf_con_fd_t  *)para;
  int con_fd = ptr_conf_con_fd -> con_fd;
  char *root = (char *)ptr_conf_con_fd -> cf.root;

  pthread_t tid = pthread_self();
  printf("NO.%u thread runs now. \n",(unsigned int) tid);

  char *buff = (char *)malloc(ONEMEGA);    
  bzero(buff, ONEMEGA);

  
  int nread = 0, n = 0;

  for(;;)
  {
     if((n = read(con_fd, buff+nread, ONEMEGA - 1)) >0 )
        nread += n;
     else if(0 == n)
       break;
     else if(-1 == n && errno == EINTR)
       continue;
     else if(-1 == n && errno == EAGAIN)
       break;
     else 
     {
       perror("some error happen when read data from the contected socket\n ");
       exit(0);
     }      
  }

  if(nread != 0)
  {
     printf("the recieved http request: \n");
     printf("%s\n",buff);

     do_http_request(con_fd,(char *)root, buff, nread);
  }


  return NULL;
}

int read_conf(char *filename, conf_t *ptr_cf, char *buf, int len)
{
  FILE *fp = fopen(filename, "r");
  if(!fp)
  {
    return -1;
  }

  int pos = 0;
  char *delim_pos;
  int line_len;
  char *cur_pos = buf + pos;

  while (fgets(cur_pos, len-pos, fp))
  {
    delim_pos = strstr(cur_pos, DELIM);
    line_len = strlen(cur_pos);

    if(cur_pos[strlen(cur_pos) - 1] == '\n')
    {
      cur_pos[strlen(cur_pos) - 1] = '\0';
    }

    if(strncmp("root",cur_pos,4) == 0 )
    {
      ptr_cf -> root = delim_pos + 1;
    }

    if(strncmp("port",cur_pos,4) == 0)
    {
      ptr_cf -> port = atoi(delim_pos + 1);
    }

    cur_pos += line_len;

  }
  fclose(fp);
  return 0;
}
