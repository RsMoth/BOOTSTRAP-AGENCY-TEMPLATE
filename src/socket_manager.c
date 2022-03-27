
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
  }

  return sfd;
}
#endif

int sm_connect_tcp(const char *hostname, int port) {
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = PF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  struct addrinfo *res0;
  char *port_str = NULL;
  if (asprintf(&port_str, "%d", port) < 0) {
    return -1;  // asprintf failed
  }
  int ret = getaddrinfo(hostname, port_str, &hints, &res0);
  free(port_str);
  if (ret) {
    perror("Unknown host");
    return (ret < 0 ? ret : -1);
  }
  ret = -1;
  int fd = 0;
  struct addrinfo *res;
  for (res = res0; res; res = res->ai_next) {
#ifdef WIN32
    if (fd != INVALID_SOCKET) {
      closesocket(fd);
    }
    fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd == INVALID_SOCKET) {
      continue;
    }
    u_long nb = 1;
    if (ioctlsocket(fd, FIONBIO, &nb) ||
        (connect(fd, res->ai_addr, res->ai_addrlen) == SOCKET_ERROR &&
         WSAGetLastError() != WSAEWOULDBLOCK &&
         WSAGetLastError() != WSAEINPROGRESS)) {
      continue;
    }

    struct timeval to;
    to.tv_sec = 0;
    to.tv_usec= 500*1000;
    fd_set write_fds;
    FD_ZERO(&write_fds);
    FD_SET(fd, &write_fds);

    if (select(1, NULL, &write_fds, NULL, &to) < 1) {
      continue;
    }
#else
    if (fd > 0) {
      close(fd);
    }
    fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
      continue;
    }
    // try non-blocking connect, usually succeeds even if unreachable
    int opts = fcntl(fd, F_GETFL);
    if (opts < 0 ||
        fcntl(fd, F_SETFL, (opts | O_NONBLOCK)) < 0 ||
        ((connect(fd, res->ai_addr, res->ai_addrlen) < 0) ==
         (errno != EINPROGRESS))) {
      continue;
    }
    // try blocking select to verify its reachable
    struct timeval to;
    to.tv_sec = 0;
    to.tv_usec= 500*1000; // arbitrary
    fd_set error_fds;
    FD_ZERO(&error_fds);
    FD_SET(fd, &error_fds);
    if (fcntl(fd, F_SETFL, opts) < 0) {
      continue;
    }
    int is_error = select(fd + 1, &error_fds, NULL, NULL, &to);
    if (is_error) {
      continue;
    }
    // success!  set back to non-blocking and return
    if (fcntl(fd, F_SETFL, (opts | O_NONBLOCK)) < 0) {
      continue;
    }
#endif
    ret = fd;
    break;
  }
#ifdef WIN32
  if (fd != INVALID_SOCKET && ret <= 0) {
    closesocket(fd);
  }
#else
  if (fd > 0 && ret <= 0) {
    close(fd);
  }
#endif
  freeaddrinfo(res0);
  return ret;
}

int sm_connect(const char *socket_addr) {
  if (strncmp(socket_addr, "unix:", 5) == 0) {
#ifdef WIN32
    return -1;
#else
    return sm_connect_unix(socket_addr + 5);
#endif
  } else {
    const char *s_port = strrchr(socket_addr, ':');
    int port = 0;

    if (s_port) {
      port = strtol(s_port + 1, NULL, 0);
    }

    if (port <= 0) {
      return -1;
    }

    size_t host_len = s_port - socket_addr;
    char *host = strndup(socket_addr, host_len);

    int ret = sm_connect_tcp(host, port);
    free(host);
    return ret;
  }
}


sm_status sm_on_debug(sm_t self, const char *format, ...) {
  if (self->is_debug && *self->is_debug) {
    va_list args;
    va_start(args, format);
    vfprintf(stdout, format, args);
    fprintf(stdout, "\n");
    va_end(args);
  }
  return SM_SUCCESS;
}

sm_status sm_add_fd(sm_t self, int fd, void *ssl_session, void *value,
    bool is_server) {
  sm_private_t my = self->private_state;
  if (FD_ISSET(fd, my->all_fds)) {
    return SM_ERROR;
  }
  if (ht_put(my->fd_to_value, HT_KEY(fd), value)) {
    // The above FD_ISSET(..master..) should prevent this
    return SM_ERROR;
  }
  if (ssl_session != NULL && ht_put(my->fd_to_ssl, HT_KEY(fd), ssl_session)) {
    return SM_ERROR;
  }
  // is_server == getsockopt(..., SO_ACCEPTCONN, ...)?
  sm_on_debug(self, "ss.add%s_fd(%d)", (is_server ? "_server" : ""), fd);
  FD_SET(fd, my->all_fds);
  FD_CLR(fd, my->send_fds); // only set if blocked
  FD_SET(fd, my->recv_fds);
  FD_CLR(fd, my->tmp_send_fds);
  FD_CLR(fd, my->tmp_recv_fds);
  FD_CLR(fd, my->tmp_fail_fds);
  if (is_server) {
    FD_SET(fd, my->server_fds);
  }
  if (fd > my->max_fd) {
    my->max_fd = fd;
  }
  return SM_SUCCESS;
}

sm_status sm_remove_fd(sm_t self, int fd) {
  sm_private_t my = self->private_state;
  if (!FD_ISSET(fd, my->all_fds)) {
    return SM_ERROR;
  }
  ht_put(my->fd_to_ssl, HT_KEY(fd), NULL);
  void *value = ht_put(my->fd_to_value, HT_KEY(fd), NULL);
  bool is_server = FD_ISSET(fd, my->server_fds);
  sm_on_debug(self, "ss.remove%s_fd(%d)", (is_server ? "_server" : ""), fd);
  sm_status ret = self->on_close(self, fd, value, is_server);
#ifdef WIN32
  closesocket(fd);
#else
  close(fd);
#endif
  FD_CLR(fd, my->all_fds);
  if (is_server) {
    FD_CLR(fd, my->server_fds);
  }
  FD_CLR(fd, my->send_fds);
  FD_CLR(fd, my->recv_fds);
  FD_CLR(fd, my->tmp_send_fds);
  FD_CLR(fd, my->tmp_recv_fds);
  FD_CLR(fd, my->tmp_fail_fds);
  if (fd == my->max_fd) {
    while (my->max_fd >= 0 && !FD_ISSET(my->max_fd, my->all_fds)) {
      my->max_fd--;
    }
  }
  if (ht_size(my->fd_to_sendq)) {
    sm_sendq_t *qs = (sm_sendq_t *)ht_values(my->fd_to_sendq);
    sm_sendq_t *q;
    for (q = qs; *q; q++) {
      sm_sendq_t sendq = *q;
      while (sendq) {
        if (sendq->recv_fd == fd) {
          sendq->recv_fd = 0;
          // don't abort this blocked send, even though the "cause" has ended
        }
        sendq = sendq->next;
      }
    }
    free(qs);
  }
  return ret;
}

sm_status sm_send(sm_t self, int fd, const char *data, size_t length,
    void* value) {
  sm_private_t my = self->private_state;
  sm_sendq_t sendq = (sm_sendq_t)ht_get_value(my->fd_to_sendq, HT_KEY(fd));
  const char *head = data;
  const char *tail = data + length;
  if (!sendq) {
    void *ssl_session = ht_get_value(my->fd_to_ssl, HT_KEY(fd));
    // send as much as we can without blocking
    while (1) {
      ssize_t sent_bytes;
      if (ssl_session == NULL) {
        sent_bytes = send(fd, (void*)head, (tail - head), 0);
        if (sent_bytes <= 0) {
#ifdef WIN32
          if (sent_bytes && WSAGetLastError() != WSAEWOULDBLOCK) {
#else
          if (sent_bytes && errno != EWOULDBLOCK) {
#endif
            sm_on_debug(self, "ss.failed fd=%d", fd);
            perror("send failed");
            return SM_ERROR;
          }
          break;
        }
      } else {
        sent_bytes = SSL_write((SSL *)ssl_session, (void*)head, tail - head);
        if (sent_bytes <= 0) {
          if (SSL_get_error(ssl_session, sent_bytes) != SSL_ERROR_WANT_READ &&
              SSL_get_error(ssl_session, sent_bytes) != SSL_ERROR_WANT_WRITE) {
            sm_on_debug(self, "ss.failed fd=%d", fd);
            perror("ssl send failed");
            return SM_ERROR;
          }
          break;
        }
      }
      head += sent_bytes;
      if (head >= tail) {
        self->on_sent(self, fd, value, data, length);