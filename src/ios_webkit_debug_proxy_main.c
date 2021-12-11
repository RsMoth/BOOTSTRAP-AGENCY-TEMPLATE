
// Google BSD license https://developers.google.com/google-bsd-license
// Copyright 2012 Google Inc. wrightt@google.com

//
// This "main" connects the debugger to our socket management backend.
//

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE
#include <getopt.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef HAVE_REGEX_H
#include <pcre.h>
#include <pcreposix.h>
#else
#include <regex.h>
#endif

#ifdef WIN32
#include <winsock2.h>
#endif

#include "device_listener.h"
#include "hash_table.h"
#include "ios_webkit_debug_proxy.h"
#include "port_config.h"
#include "socket_manager.h"
#include "webinspector.h"
#include "websocket.h"


struct iwdpm_struct {
  char *config;
  char *frontend;
  char *sim_wi_socket_addr;
  bool is_debug;

  pc_t pc;
  sm_t sm;
  iwdp_t iwdp;
};
typedef struct iwdpm_struct *iwdpm_t;
iwdpm_t iwdpm_new();
void iwdpm_free(iwdpm_t self);

int iwdpm_configure(iwdpm_t self, int argc, char **argv);

void iwdpm_create_bridge(iwdpm_t self);

static int quit_flag = 0;

static void on_signal(int sig) {
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
#endif

  iwdpm_t self = iwdpm_new();
  int ret = iwdpm_configure(self, argc, argv);
  if (ret) {
    exit(ret > 0 ? ret : 0);
    return ret;
  }

  iwdpm_create_bridge(self);

  iwdp_t iwdp = self->iwdp;
  if (iwdp->start(iwdp)) {
    return -1;// TODO cleanup
  }

  sm_t sm = self->sm;
  while (!quit_flag) {
    if (sm->select(sm, 2) < 0) {
      ret = -1;
      break;
    }
  }
  sm->cleanup(sm);
  iwdpm_free(self);
#ifdef WIN32
  WSACleanup();
#endif
  return ret;
}
//
// Connect ios_webkit_debug_proxy to socket_selector/etc:
//

int iwdpm_subscribe(iwdp_t iwdp) {
  return dl_connect(-1);
}
int iwdpm_attach(iwdp_t iwdp, const char *device_id, char **to_device_id,
    char **to_device_name, int *to_device_os_version, void **to_ssl_session) {
  return wi_connect(device_id, to_device_id, to_device_name,
      to_device_os_version, to_ssl_session, -1);
}
iwdp_status iwdpm_select_port(iwdp_t iwdp, const char *device_id,
    int *to_port, int *to_min_port, int *to_max_port) {
  iwdpm_t self = (iwdpm_t)iwdp->state;
  int ret = 0;
  // reparse every time, in case the file has changed
  int is_file = 0;
  if (!self->pc) {
    self->pc = pc_new();
    if (pc_add_line(self->pc, self->config, strlen(self->config))) {
      pc_clear(self->pc);
      pc_add_file(self->pc, self->config);
      is_file = 1;
    }
  }
  ret = pc_select_port(self->pc, device_id, to_port, to_min_port,to_max_port);
  if (is_file) {
    pc_free(self->pc);
    self->pc = NULL;
  }
  return (ret ? IWDP_ERROR : IWDP_SUCCESS);
}
int iwdpm_listen(iwdp_t iwdp, int port) {
  return sm_listen(port);
}
int iwdpm_connect(iwdp_t iwdp, const char *socket_addr) {
  return sm_connect(socket_addr);
}
iwdp_status iwdpm_send(iwdp_t iwdp, int fd, const char *data, size_t length) {
  sm_t sm = ((iwdpm_t)iwdp->state)->sm;
  return sm->send(sm, fd, data, length, NULL);
}
iwdp_status iwdpm_add_fd(iwdp_t iwdp, int fd, void *ssl_session, void *value,
    bool is_server) {
  sm_t sm = ((iwdpm_t)iwdp->state)->sm;
  return sm->add_fd(sm, fd, ssl_session, value, is_server);
}
iwdp_status iwdpm_remove_fd(iwdp_t iwdp, int fd) {
  sm_t sm = ((iwdpm_t)iwdp->state)->sm;
  return sm->remove_fd(sm, fd);
}