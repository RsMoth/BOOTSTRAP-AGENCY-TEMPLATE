
// Google BSD license https://developers.google.com/google-bsd-license
// Copyright 2012 Google Inc. wrightt@google.com

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#endif

#include <openssl/ssl.h>

#include "char_buffer.h"
#include "socket_manager.h"
#include "hash_table.h"
#include "strndup.h"

#if defined(__MACH__) || defined(WIN32)
#define SIZEOF_FD_SET sizeof(struct fd_set)
#define RECV_FLAGS 0
#else
#define SIZEOF_FD_SET sizeof(fd_set)
#define RECV_FLAGS MSG_DONTWAIT
#endif

struct sm_private {
  struct timeval timeout;
  // fds:
  fd_set *all_fds;
  int max_fd;  // max fd in all_fds
  // subsets of all_fds:
  fd_set *server_fds; // can on_accept, i.e. "is_server"
  fd_set *send_fds;   // blocked sends, same as fd_to_sendq.keys
  fd_set *recv_fds;   // can recv, same as all_fds - sendq.recv_fd's
  // fd to ssl_session
  ht_t fd_to_ssl;
  // fd to on_* callback
  ht_t fd_to_value;
  // fd to blocked sm_sendq_t, often empty
  ht_t fd_to_sendq;
  // temp recv buffer, for use in sm_select:
  char *tmp_buf;
  size_t tmp_buf_length;
  // temp fd sets, for use in sm_select:
  fd_set *tmp_send_fds;
  fd_set *tmp_recv_fds;
  fd_set *tmp_fail_fds;
  // current sm_select on_recv fd, only set when in sm_select loop
  int curr_recv_fd;
};

struct sm_sendq;
typedef struct sm_sendq *sm_sendq_t;
struct sm_sendq {
  void *value;  // for on_sent
  int recv_fd;  // the my->recv_fd that caused this blocked send
  char *begin;  // sm_send data
  char *head;
  char *tail;   // begin + sm_send length
  sm_sendq_t next;
};
sm_sendq_t sm_sendq_new(int recv_fd, void *value, const char *data,
    size_t length);
void sm_sendq_free(sm_sendq_t sendq);


int sm_listen(int port) {
  int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#ifdef WIN32
  if (fd == INVALID_SOCKET) {
    fprintf(stderr, "socket_manager: socket function failed with\
        error %d\n", WSAGetLastError());
    return -1;
  }
  struct sockaddr_in local;
  local.sin_family = AF_INET;
  local.sin_addr.s_addr = INADDR_ANY;
  local.sin_port = htons(port);
  int ra = 1;
  u_long nb = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&ra,
        sizeof(ra)) == SOCKET_ERROR ||
      ioctlsocket(fd, FIONBIO, &nb) ||
      bind(fd, (SOCKADDR *)&local, sizeof(local)) == SOCKET_ERROR ||
      listen(fd, 5)) {
    fprintf(stderr, "socket_manager: bind failed with\
        error %d\n", WSAGetLastError());
    closesocket(fd);
    return -1;
  }
#else
  if (fd < 0) {
    return -1;
  }
  int opts = fcntl(fd, F_GETFL);
  struct sockaddr_in local;
  local.sin_family = AF_INET;
  local.sin_addr.s_addr = INADDR_ANY;
  local.sin_port = htons(port);
  int ra = 1;
  int nb = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&ra,sizeof(ra)) < 0 ||
      opts < 0 ||
      ioctl(fd, FIONBIO, (char *)&nb) < 0 ||
      bind(fd, (struct sockaddr*)&local, sizeof(local)) < 0 ||
      listen(fd, 5)) {
    close(fd);
    return -1;
  }
#endif
  return fd;
}

#ifndef WIN32
int sm_connect_unix(const char *filename) {
  struct sockaddr_un name;
  int sfd = -1;
  struct stat fst;

  if (stat(filename, &fst) != 0 || !S_ISSOCK(fst.st_mode)) {
    fprintf(stderr, "File '%s' is not a socket: %s\n", filename,
        strerror(errno));
    return -1;
  }

  if ((sfd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
    perror("create socket failed");
    return -1;
  }

  int opts = fcntl(sfd, F_GETFL);
  if (fcntl(sfd, F_SETFL, (opts | O_NONBLOCK)) < 0) {
    perror("failed to set socket to non-blocking");
    return -1;
  }

  name.sun_family = AF_UNIX;
  strncpy(name.sun_path, filename, sizeof(name.sun_path) - 1);

  if (connect(sfd, (struct sockaddr*)&name, sizeof(name)) < 0) {
    close(sfd);
    perror("connect failed");
    return -1;