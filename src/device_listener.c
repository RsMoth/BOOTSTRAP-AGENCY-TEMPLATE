// Google BSD license https://developers.google.com/google-bsd-license
// Copyright 2012 Google Inc. wrightt@google.com

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE
#include <errno.h>
#include <getopt.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef WIN32
#include <winsock2.h>
#else
#include <resolv.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#endif

#include <plist/plist.h>

#include "char_buffer.h"
#include "hash_table.h"
#include "device_listener.h"

//
// We can't use libusbmuxd's
//     int usbmuxd_subscribe(usbmuxd_event_cb_t callback, void *user_data)
// because it's threaded and does blocking reads, but we want a
// select-friendly fd that we can loop-unroll.  Fortunately this is relatively
// straight-forward.
//

#define USBMUXD_SOCKET_PORT 27015
#define USBMUXD_FILE_PATH "/var/run/usbmuxd"
#define TYPE_PLIST 8
#define LIBUSBMUX_VERSION 3

struct dl_private {
  cb_t in;
  ht_t device_num_to_device_id;
  bool has_length;
  size_t body_length;
};

int dl_connect(int recv_timeout) {
  int fd = -1;
#ifdef WIN32
  fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (fd == INVALID_SOCKET) {
    fprintf(stderr, "device_listener: socket function failed wi