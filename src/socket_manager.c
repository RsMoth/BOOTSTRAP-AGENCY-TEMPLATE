
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