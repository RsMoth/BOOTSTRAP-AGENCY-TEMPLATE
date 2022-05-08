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

// some arbitrarly limit, to catch bad packets
#define MAX_BODY_LENGTH 1<<26

struct wi_private {
  bool partials_supported;
  cb_t in;
  cb_t partial;
  bool has_length;
  size_t body_length;
};

//
// CONNECT
//

// based on latest libimobiledevice/src/idevice.h
struct idevice_connection_private {
  idevice_t device;
  enum idevice_connection_type type;
  void *data;
  void *ssl_data;
};

struct ssl_data_private {
	SSL *session;
	SSL_CTX *ctx;
};
typedef struct ssl_data_private *ssl_data_t;

wi_status idevice_connection_get_ssl_session(idevice_connection_t connection,
    SSL **to_session) {
  if (!connection || !to_session) {
    return WI_ERROR;
  }

  idevice_connection_private *c = (
      (sizeof(*connection) == sizeof(idevice_connection_private)) ?
      (idevice_connection_private *) connection : NULL);

  if (!c || c->data <= 0) {
    perror("Invalid idevice_connection struct. Please verify that "
        __FILE__ "'s idevice_connection_private matches your version of"
        " libimbiledevice/src/idevice.h");
    return WI_ERROR;
  }

  ssl_data_t sd = (ssl_data_t)c->ssl_data;
  if (!sd || !sd->session) {
    perror("Invalid ssl_data struct. Make sure l