
// Google BSD license https://developers.google.com/google-bsd-license
// Copyright 2012 Google Inc. wrightt@google.com

//
// A minimal webinspector client
//

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

#include "ios-webkit-debug-proxy/webinspector.h"

// our state
struct my_wi_struct {
  char *device_id;
  int fd;
  wi_t wi;
};
typedef struct my_wi_struct *my_wi_t;

//
// inspector callbacks:
//

wi_status send_packet(wi_t wi, const char *packet, size_t length) {
  my_wi_t my_wi = (my_wi_t)wi->state;
  ssize_t sent_bytes = send(my_wi->fd, (void*)packet, length, 0);
  return (sent_bytes == length ? WI_SUCCESS : WI_ERROR);
}