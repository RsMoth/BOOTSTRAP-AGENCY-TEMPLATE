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
      goto leave_cleanup;
    }
#endif
  } else {
    long millis = (recv_timeout > 0 ? recv_timeout : 5000);
    struct timeval tv;
    tv.tv_sec = (time_t) (millis / 1000);
    tv.tv_usec = (time_t) ((millis - (tv.tv_sec * 1000)) * 1000);
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,
          sizeof(tv))) {
      perror("Could not set socket receive timeout");
      goto leave_cleanup;
    }
  }

  // success
  ret = fd;

leave_cleanup:
#ifdef WIN32
  if (ret < 0 && fd != INVALID_SOCKET) {
    closesocket(fd);
  }
#else
  if (ret < 0 && fd > 0) {
    close(fd);
  }
#endif
  // don't call usbmuxd_disconnect(fd)!
  //idevice_disconnect(connection);
  free(connection);
  lockdownd_client_free(client);
  idevice_free(phone);
  return ret;
}

//
// SEND
//

wi_status wi_on_error(wi_t self, const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  fprintf(stderr, "\n");
  va_end(args);
  return WI_ERROR;
}

wi_status wi_on_debug(wi_t self, const char *message,
    const char *buf, size_t length) {
  if (self->is_debug && *self->is_debug) {
    char *text;
    cb_asprint(&text, buf, length, 80, 30);
    printf("%s[%zd]:\n%s\n", message, length, text);
    free(text);
  }
  return WI_SUCCESS;
}

/*
   WIRFinalMessageKey
   __selector
   __argument
 */
wi_status wi_send_plist(wi_t self, plist_t rpc_dict) {
  wi_private_t my = self->private_state;
  char *rpc_bin = NULL;
  uint32_t rpc_len = 0;
  plist_to_bin(rpc_dict, &rpc_bin, &rpc_len);
  // if our message is <8k, we'll send a single final_msg,
  // otherwise we'll send <8k partial_msg "chunks" then a final_msg "chunk"
  wi_status ret = WI_ERROR;
  uint32_t i;
  for (i = 0; ; i += MAX_RPC_LEN) {
    bool is_partial = false;
    char *data = NULL;
    uint32_t data_len = 0;
    if (!my->partials_supported) {
      data = rpc_bin;
      data_len = rpc_len;
      rpc_bin = NULL;
    } else {
      is_partial = (rpc_len - i > MAX_RPC_LEN);
      plist_t wi_dict = plist_new_dict();
      plist_t wi_rpc = plist_new_data(rpc_bin + i,
          (is_partial ? MAX_RPC_LEN : rpc_len - i));
      plist_dict_set_item(wi_dict,
          (is_partial ? "WIRPartialMessageKey" : "WIRFinalMessageKey"), wi_rpc);
      plist_to_bin(wi_dict, &data, &data_len);
      plist_free(wi_dict);
      wi_dict = NULL;
      wi_rpc = NULL; // freed by wi_dict
      if (!data) {
        break;
      }
    }

    size_t length = data_len + 4;
    char *out_head = (char*)malloc(length * sizeof(char));
    if (!out_head) {
      if (my->partials_supported) {
        free(data);
      }
      break;
    }
    char *out_tail = out_head;

    // write big-endian int
    *out_tail++ = ((data_len >> 24) & 0xFF);
    *out_tail++ = ((data_len >> 16) & 0xFF);
    *out_tail++ = ((data_len >> 8) & 0xFF);
    *out_tail++ = (data_len & 0xFF);

    // write data
    memcpy(out_tail, data, data_len);
    free(data);

    wi_on_debug(self, "wi.send_packet", out_head, length);
    wi_status not_sent = self->send_packet(self, out_head, length);
    free(out_head);
    if (not_sent) {
      break;
    }

    if (!is_partial) {
      ret = WI_SUCCESS;
      break;
    }
  }
  free(rpc_bin);
  return ret;
}

//
// RECV
//

wi_status wi_parse_length(wi_t self, const char *buf, size_t *to_length) {
  if (!buf || !to_length) {
    return WI_ERROR;
  }
  *to_length = (
      ((((unsigned char) buf[0]) & 0xFF) << 24) |
      ((((unsigned char) buf[1]) & 0xFF) << 16) |
      ((((unsigned char) buf[2]) & 0xFF) << 8) |
      (((unsigned char) buf[3]) & 0xFF));
  if (MAX_BODY_LENGTH > 0 && *to_length > MAX_BODY_LENGTH) {
#define TO_CHAR(c) ((c) >= ' ' && (c) < '~' ? (c) : '.')
    return self->on_error(self, "Invalid packet header "
        "0x%x%x%x%x == %c%c%c%c == %zd",
        buf[0], buf[1], buf[2], buf[3],
        TO_CHAR(buf[0]), TO_CHAR(buf[1]),
        TO_CHAR(buf[2]), TO_CHAR(buf[3]),
        *to_length);
  }
  return WI_SUCCESS;
}

wi_status wi_parse_plist(wi_t self, const char *from_buf, size_t length,
    plist_t *to_rpc_dict, bool *to_is_partial) {
  wi_private_t my = self->private_state;
  *to_is_partial = false;
  *to_rpc_dict = NULL;

  if (!my->partials_supported) {
    plist_from_bin(from_buf, length, to_rpc_dict);
  } else {
    plist_t wi_dict = NULL;
    plist_from_bin(from_buf, length, &wi_dict);
    if (!wi_dict) {
      return WI_ERROR;
    }
    plist_t wi_rpc = plist_dict_get_item(wi_dict, "WIRFinalMessageKey");
    if (!wi_rpc) {
      wi_rpc = plist_dict_get_item(wi_dict, "WIRPartialMessageKey");
      if (!wi_rpc) {
        return WI_ERROR;
      }
      *to_is_partial = true;
    }

    uint64_t rpc_len = 0;
    char *rpc_bin = NULL;
    plist_get_data_val(wi_rpc, &rpc_bin, &rpc_len);
    plist_free(wi_dict); // also frees wi_rpc
    if (!rpc_bin) {
      return WI_ERROR;
    }
    // assert rpc_len < MAX_RPC_LEN?

    size_t p_length = my->partial->tail - my->partial->head;
    if (*to_is_partial || p_length) {
      if (cb_ensure_capacity(my->partial, rpc_len)) {
        return self->on_error(self, "Out of memory");
      }
      memcpy(my->partial->tail, rpc_bin, rpc_len);
      my->partial->tail += rpc_len;
      p_length += rpc_len;
      free(rpc_bin);
      if (*to_is_partial) {
        return WI_SUCCESS;
      }
    }

    if (p_length) {
      plist_from_bin(my->partial->head, (uint32_t)p_length, to_rpc_dict);
      cb_clear(my->partial);
    } else {
      plist_from_bin(rpc_bin, (uint32_t)rpc_len, to_rpc_dict);
      free(rpc_bin);
    }
  }

  return (*to_rpc_dict ? WI_SUCCESS : WI_ERROR);
}

wi_status wi_recv_packet(wi_t self, const char *packet, ssize_t length) {
  wi_on_debug(self, "wi.recv_packet", packet, length);

  size_t body_length = 0;
  plist_t rpc_dict = NULL;
  bool is_partial = false;
  if (!packet || length < 4 || wi_parse_length(self, packet, &body_length) ||
      //TODO (body_length != length - 4) ||
      wi_parse_plist(self, packet + 4, body_length, &rpc_dict, &is_partial)) {
    // invalid packet
    char *text = NULL;
    if (body_length != length - 4) {
      if (asprintf(&text, "size %zd != %zd - 4", body_length, length) < 0) {
        return self->on_error(self, "asprintf failed");
      }
    } else {
      cb_asprint(&text, packet, length, 80, 50);
    }
    wi_status ret = self->on_error(self, "Invalid packet:\n%s\n", text);
    free(text);
    return ret;
  }

  if (is_partial) {
    return WI_SUCCESS;
  }
  wi_status ret = self->recv_plist(self, rpc_dict);
  plist_free(rpc_dict);
  return ret;
}

wi_status wi_recv_loop(wi_t self) {
  wi_private_t my = self->private_state;
  wi_status ret;
  const char *in_head = my->in->in_head;
  const char *in_tail = my->in->in_tail;
  while (1) {
    size_t in_length = in_tail - in_head;
  