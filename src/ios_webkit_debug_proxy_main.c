
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