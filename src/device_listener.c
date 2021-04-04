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
/