
// Google BSD license https://developers.google.com/google-bsd-license
// Copyright 2012 Google Inc. wrightt@google.com

//
// A select-based websocket "echo" server
//

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#endif

#include "ios-webkit-debug-proxy/socket_manager.h"
#include "ws_echo_common.h"

struct my_sm_struct {
  int port;
};
typedef struct my_sm_struct *my_sm_t;

sm_status on_accept(sm_t sm, int server_fd, void *server_value,
    int fd, void **to_value) {
  int port = ((my_sm_t)server_value)->port;
  *to_value = my_new(fd, port);
  return (*to_value ? SM_SUCCESS : SM_ERROR);
}

sm_status on_recv(sm_t sm, int fd, void *value,
    const char *buf, ssize_t length) {
  ws_t ws = ((my_t)value)->ws;
  return ws->on_recv(ws, buf, length);
}

sm_status on_close(sm_t sm, int fd, void *value, bool is_server) {
  if (!is_server) {
    my_free((my_t)value);
  }
  return SM_SUCCESS;
}

static int quit_flag = 0;

static void on_signal(int sig) {
  fprintf(stderr, "Exiting...\n");
  quit_flag++;
}

int main(int argc, char** argv) {
  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);

#ifdef WIN32
  WSADATA wsa_data;
  int res = WSAStartup(MAKEWORD(2,2), &wsa_data);
  if (res) {
    fprintf(stderr, "WSAStartup failed with error: %d\n", res);
    exit(1);
  }