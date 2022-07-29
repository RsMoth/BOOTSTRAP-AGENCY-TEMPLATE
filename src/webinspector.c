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
    perror("Invalid ssl_data struct. Make sure libimobiledevice was compiled"
        " with openssl. Otherwise please verify that " __FILE__ "'s ssl_data"
        " matches your version of libimbiledevice/src/idevice.h");
    return WI_ERROR;
  }

  *to_session = sd->session;
  return WI_SUCCESS;
}

int wi_connect(const char *device_id, char **to_device_id,
    char **to_device_name, int *to_device_os_version,
    void **to_ssl_session, int recv_timeout) {
  int ret = -1;

  idevice_t phone = NULL;
  plist_t node = NULL;
  lockdownd_service_descriptor_t service = NULL;
  lockdownd_client_t client = NULL;
  idevice_connection_t connection = NULL;
  int fd = -1;
  SSL *ssl_session = NULL;

  // get phone
  if (idevice_new_with_options(&phone, device_id, IDEVICE_LOOKUP_USBMUX | IDEVICE_LOOKUP_NETWORK)) {
    fprintf(stderr, "No device found, is it plugged in?\n");
    goto leave_cleanup;
  }

  // connect to lockdownd
  lockdownd_error_t ldret;
  if (LOCKDOWN_E_SUCCESS != (ldret = lockdownd_client_new_with_handshake(
        phone, &client, "ios_webkit_debug_proxy"))) {
    fprintf(stderr, "Could not connect to lockdownd, error code %d. Exiting.\n", ldret);
    goto leave_cleanup;
  }

  // get device info
  if (to_device_id &&
      !lockdownd_get_value(client, NULL, "UniqueDeviceID", &node)) {
    plist_get_string_val(node, to_device_id);
    plist_free(node);
    node = NULL;
  }
  if (to_device_name &&
      !lockdownd_get_value(client, NULL, "DeviceName", &node)) {
    plist_get_string_val(node, to_device_name);
    plist_free(node);
    node = NULL;
  }
  if (to_device_os_version &&
      !lockdownd_get_value(client, NULL, "ProductVersion", &node)) {
    int vers[3] = {0, 0, 0};
    char *s_version = NULL;
    plist_get_string_val(node, &s_version);
    if (s_version && sscanf(s_version, "%d.%d.%d",
          &vers[0], &vers[1], &vers[2]) >= 2) {
      *to_device_os_version = ((vers[0] & 0xFF) << 16) |
                              ((vers[1] & 0xFF) << 8)  |
                               (vers[2] & 0xFF);
    } else {
      *to_device_os_version = 0;
    }
    free(s_version);
    plist_free(node);
  }

  // start webinspector, get port
  if (lockdownd_start_service(client, "com.apple.webinspector", &service) ||
      !service->port) {
    perror("Could not start com.apple.webinspector!");
    goto leave_cleanup;
  }

  // connect to webinspector
  if (idevice_connect(phone, service->port, &connection)) {
    perror("idevice_connect failed!");
    goto leave_cleanup;
  }

  // enable ssl
  if (service->ssl_enabled == 1) {
    if (!to_ssl_session || idevice_connection_enable_ssl(connection) ||
        idevice_connection_get_ssl_session(connection, &ssl_session)) {
      perror("ssl connection failed!");
      goto leave_cleanup;
    }
    *to_ssl_session = ssl_session;
  }

  if (client) {
    // not needed anymore
    lockdownd_client_free(client);
    client = NULL;
  }

  // extract the connection fd
  if (idevice_connection_get_fd(connection, &fd)) {
    perror("Unable to get connection file descriptor.");
    goto leave_cleanup;
  }

  if (recv_timeout < 0) {
#ifdef WIN32
    u_long nb = 1;
    if (ioctlsocket(fd, FIONBIO, &nb)) {
      fprintf(stderr, "webinspector: could not set socket to non-blocking");
    }
#else
    int opts = fcntl(fd, F_GETFL);
    if (!opts || fcntl(fd, F_SETFL, (opts | O_NONBLOCK)) < 0) {
      perror("Could not set socket to non-blocking");
      goto leave_cle