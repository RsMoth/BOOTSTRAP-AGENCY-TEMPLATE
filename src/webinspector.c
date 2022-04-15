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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#endif

#include <openssl/ssl.h>

#include <libimobiledevice/installation_proxy.h>
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>

#include "char_buffer.h"
#include "webinspector.h"


#define WI_DEBUG 1

// TODO figure out exact value
#define MAX_RPC_LEN 8096 - 500

// some arbitrarly limit, to catch 