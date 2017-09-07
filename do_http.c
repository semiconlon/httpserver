#include "do_http.h"
void serve_static(int fd, char *filename, size_t file_size, http_response_t *out);
const char* get_file_type(const char* type);
void do_error(int fd, char *cause, char *errnum, char* shortmsg, char *longmsg);
void http_parse_request_line(http_request_t *r);
void http_parse_request_body(http_request_t *r);
void parse_uri(char *uri, int uri_length, char *filename, char *root);

mime_type_t mime[] = 
{
  {".html", "text/html"},
  {".xml", "text/xml"},
  {".xhtml", "application/xhtml+xml"},
  {".txt", "text/plain"},
  {".rtf", "application/rtf"},
  {".pdf", "application/pdf"},
  {".word", "application/msword"},
  {".png", "image/png"},
  {".gif", "image/gif"},
  {".jpg", "image/jpeg"},
  {".jpeg", "image/jpeg"},
  {".au", "audio/basic"},
  {".mpeg", "video/mpeg"},
  {".mpg", "video/mpeg"},
  {".avi", "video/x-msvideo"},
  {".gz", "application/x-gzip"},
  {".tar", "application/x-tar"},
  {".css", "text/css"},
  {NULL ,"text/plain"}
};

void do_http_request(int con_fd,char *root, char *buff, size_t buff_size)
{
  printf("start dealing http rquest!\n");

  http_request_t *r = (http_request_t *)malloc(sizeof(http_request_t));
  r -> fd = con_fd;
  r -> buff = buff;
  r -> pos = 0;
  r -> last = r -> pos + buff_size;
  r -> state = 0;
  r -> root = root;
  INIT_LIST_HEAD(&(r -> list));

  struct stat sbuf;
  printf("initializing http_request_t \n");

  printf("start parsing http request line \n");
  http_parse_request_line(r);
  //snprintf(r -> uri, r -> uri_end - r -> uri_start, (char *)r->uri_start);
  printf("finish parsing http request line\n");
  printf("the http request method is %s.\n", (char *) r -> method);
  printf("the http http protocol major number is %d.\n", r->http_major);
  printf("the http http protocol minor number is %d.\n", r->http_minor);
  printf("the http uri is %s", r -> uri);
  printf("start paring http request body\n");
  http_parse_request_body(r);
  printf("finish parsing http request body\n");

  http_response_t *rp = (http_response_t *)malloc(sizeof(http_response_t));
  rp -> fd = r -> fd;
  rp -> keep_alive = 0;
  rp -> modified = 1;
  rp -> status = 0;

  char fin_path[1000];
  parse_uri(r -> uri_start, r -> uri_end - r -> uri_start, fin_path, r -> root);
  printf("the final file path is %s\n",fin_path);

  if(stat(fin_path, &sbuf)<0)
    do_error(r -> fd, fin_path, "404", "Not Found", "httpserver can't find the file");
  else {
    serve_static(r -> fd, fin_path,sbuf.st_size,rp);
    int file_fd = open(fin_path, O_RDONLY, 0);
    off_t  offset = 0;

    sendfile(r -> fd, file_fd, &offset, sbuf.st_size);
  }
  close(r -> fd);
  return ;
}

void serve_static(int fd, char *filename, size_t file_size, http_response_t *out)
{
  char header[1000];
  char buf[1000];
  struct tm tm;
  
  const char *dot_pos = strchr(filename, '.');
  const char *file_type = get_file_type(dot_pos);

  sprintf(header, "HTTP/1.1 %d %s \r\n", out -> status, "OK");
  if(out -> keep_alive){
  sprintf(header,"%sConnection: keep-alive\r\n", header);
  sprintf(header,"%sKeep-Alive: timeout=%d\r\n",header, 0);
  }
  if(out -> modified){
  sprintf(header, "%sContent-type: %s\r\n", header, file_type);
  sprintf(header, "%sContent-length: %zu\r\n", header, file_size);
  localtime_r(&(out->mtime), &tm);
  strftime(buf,1000,"%a,%d %b %Y %H:%M:%S GMT", &tm);
  sprintf(header, "%sLast-Modified: %s \r\n", header, buf);
  }

  sprintf(header, "%sSever: httpserver\r\n", header);
  sprintf(header, "%s\r\n", header);
  
  write(fd, header, strlen(header));
  
}


const char* get_file_type(const char* type)
{
  if(type == NULL)
  return "text/plain";

  int i;
  for(i = 0; mime[i].type != NULL; ++i){
  if(strcmp(type,mime[i].type) == 0)
    return mime[i].value;
  }
  return mime[i].value;
}
void do_error(int fd, char *cause, char *errnum, char* shortmsg, char *longmsg)
{
  char header[1000], body[10000];
  sprintf(body, "<html><title>httpserver error </title>");
  sprintf(body, "%s<body>\n",body);
  sprintf(body,"%s%s: %s\n",body, errnum, shortmsg);
  sprintf(body,"%s<p>%s:%s\n</p>",body,longmsg, cause);
  sprintf(body,"%s<hr><em>httpserver </em>\n</body></html>",body);

  sprintf(header,"HTTP/1.1 %s %s\r\n", errnum, shortmsg);
  sprintf(header,"%sServer: httpserver\r\n", header);
  sprintf(header,"%sContent-type: text/html\r\n", header);
  sprintf(header,"%sConten-length: %d\r\n\r\n", header, (int)strlen(body));

  write(fd, header, strlen(header));
  write(fd, body, strlen(body));
}

void parse_uri(char *uri, int uri_length, char *filename, char *root)
{
  uri[uri_length] = '\0';

  char *question_mark = strchr(uri, '?');
  int file_length;

  if(question_mark){
  file_length = (int)(question_mark - uri);
  }else{
  file_length = uri_length;
  }

  strcpy(filename, root);
  strncat(filename,uri,file_length);

  char *last_comp = strchr(filename, '/');
  char *last_dot = strchr(filename, '.');

  if(last_dot == NULL && filename [strlen(filename) - 1] != '/')
    strcat(filename, "/");

  if(filename[strlen(filename)-1] == '/')
    strcat(filename,"index.html");

  return;
}

void http_parse_request_line(http_request_t *r)
{
  enum{
    sw_start = 0,
    sw_method,
    sw_spaces_before_uri,
    sw_after_slash_in_uri,
    sw_http,
    sw_http_H,
    sw_http_HT,
    sw_http_HTT,
    sw_http_HTTP,
    sw_first_major_digit,
    sw_major_digit,
    sw_first_minor_digit,
    sw_minor_digit,
    sw_spaces_afer_digit,
    sw_almost_done
  } state;

  char ch, *p,  *m;
  size_t pi;

  state = r -> state;
  for(pi = r -> pos; pi < r -> last; ++pi)
  {
    p = (char *)&r -> buff[pi];
    ch = *p;

    switch(state)
    {
      case sw_start:
        r->request_start = p;
        if(ch == '\r' || ch == '\n')
        {
          break;
        }
        state = sw_method;
        break;

      case sw_method:
        if( ch == ' ')
        {
          r -> method_end = p;
          m = (char *)r -> request_start;

          switch( p - m)
          {
            case 3:
              if(strstr(m,"GET "))
              {
                r -> method = "GET";
                break;
              }
              break;
            case 4:
              if(strstr(m,"POST"))
              {
                r -> method = "POST";
                break;
              }
              break;
            default:
              r -> method = "UNKNOWN";
              break;
          }

          state = sw_spaces_before_uri;
          break;
        }
        break;

      case sw_spaces_before_uri:
        if(ch == '/')
        {
          r -> uri_start = p;
          state = sw_after_slash_in_uri;
          break;
        }

        switch(ch)
        {
          case ' ':
            break;
        }
        break;

      case sw_after_slash_in_uri:
        switch(ch){
          case ' ':
            r-> uri_end = p;
            state = sw_http;
            break;
          default:
            break;
        }
        break;

      case sw_http:
        switch(ch){
          case ' ':
            break;
          case 'H':
            state = sw_http_H;
            break;
          default:
            break;
        }
        break;

      case sw_http_H:
        switch(ch){
          case ' ':
            break;
          case 'T':
            state = sw_http_HT;
            break;
          default:
            break;
        }
        break;

      case sw_http_HT:
        switch(ch){
          case ' ':
            break;
          case 'T':
            state = sw_http_HTT;
            break;
          default:
            break;
        }
        break;

      case sw_http_HTT:
        switch(ch){
          case ' ':
            break;
          case 'P':
            state = sw_http_HTTP;
            break;
          default:
            break;
        }
        break;

      case sw_http_HTTP:
        switch(ch){
          case '/':
            state = sw_first_major_digit;
            break;
          default:
            break;
        }
        break;

      case sw_first_major_digit:
        r -> http_major = ch - '0';
        state = sw_major_digit;
        break;

      case sw_major_digit:
        if( ch == '.'){
          state = sw_first_minor_digit;
          break;
        }
        r -> http_major = r -> http_major * 10 + ch - '0';
        break;

      case sw_first_minor_digit:
        r -> http_minor = ch - '0';
        state = sw_minor_digit;
        break;

      case sw_minor_digit:
        if(ch == '\r'){
          state = sw_almost_done;
          break;
        }

        if(ch == '\n')
          goto done;

        if( ch == ' '){
          state = sw_spaces_afer_digit;
          break;
        }
        r -> http_minor = r -> http_minor * 10 + ch - '0';
        break;

      case sw_spaces_afer_digit:
        switch(ch){
          case ' ':
            break;
          case '\r':
            state = sw_almost_done;
            break;
          case '\n':
            goto done;
          default:
            break;
        }
        break;

      case sw_almost_done:
        r -> request_end = p -1;
        switch (ch){
          case '\n':
            goto done;
          default:
            break;
        }
    }

  }

  r -> pos = pi;
  r ->state = state;

  return ;

done:
  r -> pos = pi + 1;
  if(r -> request_end == NULL){
    r -> request_end = p;
  }
  r -> state = sw_start;

  return ;
}

void http_parse_request_body(http_request_t *r)
{
  char ch, *p;
  size_t pi;

  enum{
    sw_start = 0,
    sw_key,
    sw_spaces_before_colon,
    sw_spaces_after_colon,
    sw_value,
    sw_cr,
    sw_crlf,
    sw_crlfcr,
  }  
  state;
  state = r -> state;

  http_header_t  *hd;
  for(pi = r -> pos; pi < r -> last; ++pi){
    p = (char *)&r -> buff;
    char *p;
    switch(state){
      case sw_start:
        if(ch == '\r' || ch == '\n'){
          break;
        }
        r -> cur_header_key_start = p;
        state = sw_key;
        break;

      case sw_key:
        if(ch == ' '){
          r -> cur_header_key_end = p;
          state = sw_spaces_before_colon;
          break;
        }

        if(ch == ':'){
          r -> cur_header_key_end = p;
          state = sw_spaces_after_colon;
          break;
        }
        break;

      case sw_spaces_before_colon:
        if(ch == ' '){
          break;
        } else if(ch == ':'){
          state = sw_spaces_after_colon;
          break;
        } 

      case sw_spaces_after_colon:
        if( ch == ' '){
          break;
        }
        state = sw_value;
        r -> cur_header_value_start = p;
        break;

      case sw_value:
        if(ch == '\r'){
          r -> cur_header_key_end = p;
          state = sw_cr;
        }
        if(ch == '\n'){
          r -> cur_header_value_end = p;
          state = sw_crlf;
        }
        break;

      case sw_cr:
        if( ch == '\n'){
          state = sw_crlf;
          hd = (http_header_t *)malloc(sizeof(http_header_t));
          hd -> key_start = r -> cur_header_key_start;
          hd -> key_end = r -> cur_header_key_end;
          hd -> value_start = r -> cur_header_value_start;
          hd -> value_end = r -> cur_header_value_end;

          list_add(&(hd -> list), &(r -> list));
          break;
        }

      case sw_crlf:
        if(ch == '\n'){
          state = sw_crlfcr;
        } else{
          r -> cur_header_key_start = p;
          state = sw_key;
        }
        break;

      case sw_crlfcr:
        switch(ch){
          case '\n':
            goto done;
          default:
            break;
        }
        break;
    }
  }

  r -> pos = pi;
  r -> state = state;
  return ;

done:
  r -> pos = pi + 1;
  r -> state = sw_start;

  return ;
}


