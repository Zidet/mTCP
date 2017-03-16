#ifndef __MTCP_CLIENT__

#define __MTCP_CLIENT__

/* Library Function of mtcp */
void mtcp_connect(int socket_fd, struct sockaddr_in *server_addr);
int mtcp_write(int socket_fd, unsigned char *buf, int buf_len);
void mtcp_close(int socket_fd);
static void *send_thread();
static void *receive_thread();
#endif // __MTCP_CLIENT__
