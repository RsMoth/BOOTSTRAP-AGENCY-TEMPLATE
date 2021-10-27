
// Google BSD license https://developers.google.com/google-bsd-license
// Copyright 2012 Google Inc. wrightt@google.com

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "char_buffer.h"
#include "device_listener.h"
#include "hash_table.h"
#include "ios_webkit_debug_proxy.h"
#include "rpc.h"
#include "webinspector.h"
#include "websocket.h"
#include "strndup.h"


struct iwdp_idl_struct;
typedef struct iwdp_idl_struct *iwdp_idl_t;

struct iwdp_private {
  // our device listener
  iwdp_idl_t idl;

  // our null-id registry (:9221) plus per-device ports (:9222-...)
  ht_t device_id_to_iport;

  // frontend url, e.g. "http://bar.com/devtools.html" or "/foo/inspector.html"
  char *frontend;
  char *sim_wi_socket_addr;
};


#define TYPE_IDL   1
#define TYPE_IPORT 2
#define TYPE_IWI   3
#define TYPE_IWS   4
#define TYPE_IFS   5

/*!
 * Struct type id, for iwdp_on_recv/etc "switch" use.
 *
 * Each sub-struct has a "*_fd".
 */
typedef struct {
  int type;
} iwdp_type_struct;
typedef iwdp_type_struct *iwdp_type_t;

/*!
 * Device add/remove listener.
 */
struct iwdp_idl_struct {
  iwdp_type_struct type;
  iwdp_t self;

  dl_t dl;
  int dl_fd;
};
iwdp_idl_t iwdp_idl_new();
void iwdp_idl_free(iwdp_idl_t idl);

struct iwdp_iwi_struct;
typedef struct iwdp_iwi_struct *iwdp_iwi_t;

/*!
 * browser listener.
 */
struct iwdp_iport_struct {
  iwdp_type_struct type;
  iwdp_t self;

  // browser port, e.g. 9222
  int port;
  int s_fd;

  // true if iwdp_on_attach has succeeded and we should restore this port
  // if the device is reattach
  bool is_sticky;

  // all websocket clients on this port
  // key owned by iws->ws_id
  ht_t ws_id_to_iws;

  // iOS device_id, e.g. ddc86a518cd948e13bbdeadbeef00788ea35fcf9
  char *device_id;
  char *device_name;
  int device_os_version;

  // null if the device is detached
  iwdp_iwi_t iwi;
};

typedef struct iwdp_iport_struct *iwdp_iport_t;
iwdp_iport_t iwdp_iport_new();
void iwdp_iport_free(iwdp_iport_t iport);
char *iwdp_iports_to_text(iwdp_iport_t *iports, bool want_json,
    const char *host);

/*!
 * WebInpsector.
 */
struct iwdp_iwi_struct {
  iwdp_type_struct type;
  iwdp_iport_t iport; // owner

  // webinspector
  wi_t wi;
  int wi_fd;
  char *connection_id;

  rpc_t rpc;  // plist parser
  rpc_app_t app;

  bool connected;
  uint32_t max_page_num; // > 0
  ht_t app_id_to_true;   // set of app_ids
  ht_t page_num_to_ipage;
};

iwdp_iwi_t iwdp_iwi_new(bool partials_supported, bool *is_debug);
void iwdp_iwi_free(iwdp_iwi_t iwi);

struct iwdp_ifs_struct;
typedef struct iwdp_ifs_struct *iwdp_ifs_t;

struct iwdp_ipage_struct;
typedef struct iwdp_ipage_struct *iwdp_ipage_t;

/*!
 * WebSocket connection.
 */
struct iwdp_iws_struct {
  iwdp_type_struct type;
  iwdp_iport_t iport; // owner

  // browser client
  int ws_fd;
  ws_t ws;
  char *ws_id; // devtools sender_id

  // set if the resource is /devtools/page/<page_num>
  uint32_t page_num;

  // assert (!page_num ||
  //     (ipage && ipage->page_num == page_num && ipage->iws == this))
  //
  // shortcut pointer to the page with our page_num, but only if
  // we own that page (i.e. ipage->iws == this).  Another iws can "steal" our
  // page away and set ipage == NULL, but we keep our page_num so we can report
  // a useful error.
  iwdp_ipage_t ipage; // owner is iwi->page_num_to_ipage

  // set if the resource is /devtools/<non-page>
  iwdp_ifs_t ifs;
};
typedef struct iwdp_iws_struct *iwdp_iws_t;
iwdp_iws_t iwdp_iws_new(bool *is_debug);
void iwdp_iws_free(iwdp_iws_t iws);

/*!
 * Static file-system page request.
 */
struct iwdp_ifs_struct {
  iwdp_type_struct type;
  iwdp_iws_t iws; // owner

  // static server
  int fs_fd;
};

iwdp_ifs_t iwdp_ifs_new();
void iwdp_ifs_free(iwdp_ifs_t ifs);


// page info
struct iwdp_ipage_struct {
  // browser
  uint32_t page_num;

  // webinspector, which can lag re: on_applicationSentListing
  char *app_id;
  uint32_t page_id;
  char *connection_id;
  char *title;
  char *url;
  char *sender_id;

  // set if being inspected, limit one client per page
  // owner is iport->ws_id_to_iws
  iwdp_iws_t iws;
};

iwdp_ipage_t iwdp_ipage_new();
void iwdp_ipage_free(iwdp_ipage_t ipage);
int iwdp_ipage_cmp(const void *a, const void *b);
char *iwdp_ipages_to_text(iwdp_ipage_t *ipages, bool want_json,
    const char *device_id, const char *device_name,
    const char *frontend_url, const char *host, int port);

// file extension to Content-Type
const char *EXT_TO_MIME[][2] = {
  {"css", "text/css"},
  {"gif", "image/gif; charset=binary"},
  {"html", "text/html; charset=UTF-8"},
  {"ico", "image/x-icon"},
  {"js", "application/javascript"},
  {"json", "application/json; charset=UTF-8"},
  {"png", "image/png; charset=binary"},
  {"txt", "text/plain"},
};
iwdp_status iwdp_get_content_type(const char *path, bool is_local,
    char **to_mime);

ws_status iwdp_start_devtools(iwdp_ipage_t ipage, iwdp_iws_t iws);
ws_status iwdp_stop_devtools(iwdp_ipage_t ipage);

int iwdp_update_string(char **old_value, const char *new_value);

//
// logging
//

iwdp_status iwdp_on_error(iwdp_t self, const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  fprintf(stderr, "\n");
  va_end(args);
  return IWDP_ERROR;
}

void iwdp_log_connect(iwdp_iport_t iport) {
  if (iport->device_id) {
    printf("Connected :%d to %s (%s)\n", iport->port, iport->device_name,
        iport->device_id);
  } else {
    printf("Listing devices on :%d\n", iport->port);
  }
}

void iwdp_log_disconnect(iwdp_iport_t iport) {
  if (iport->iwi && iport->iwi->connected) {
    printf("Disconnected :%d from %s (%s)\n", iport->port,
        iport->device_name, iport->device_id);
  } else {
    printf("Unable to connect to %s (%s)\n  Please"
        " verify that Settings > Safari > Advanced > Web Inspector = ON\n",
        iport->device_name, iport->device_id);
  }
}


//
// device_listener
//

dl_status iwdp_listen(iwdp_t self, const char *device_id) {
  iwdp_private_t my = self->private_state;

  // see if this device was previously attached
  ht_t iport_ht = my->device_id_to_iport;
  iwdp_iport_t iport = (iwdp_iport_t)ht_get_value(iport_ht, device_id);
  if (iport && iport->s_fd > 0) {
    return self->on_error(self, "%s already on :%d", device_id,
        iport->port);
  }
  int port = (iport ? iport->port : -1);

  // select new port
  int min_port = -1;
  int max_port = -1;
  if (self->select_port && self->select_port(self, device_id,
        &port, &min_port, &max_port)) {
    return (device_id ? DL_ERROR : DL_SUCCESS);
  }
  if (port < 0 && (min_port < 0 || max_port < min_port)) {
    return (device_id ? DL_ERROR : DL_SUCCESS); // ignore this device
  }
  if (!iport) {
    iport = iwdp_iport_new();
    iport->device_id = (device_id ? strdup(device_id) : NULL);
    ht_put(iport_ht, iport->device_id, iport);
  }
  iport->self = self;

  // listen for browser clients
  int s_fd = -1;
  if (port > 0) {
    s_fd = self->listen(self, port);
  }
  if (s_fd < 0 && (min_port > 0 && max_port >= min_port)) {
    iwdp_iport_t *iports = (iwdp_iport_t *)ht_values(iport_ht);
    int p;
    for (p = min_port; p <= max_port; p++) {
      bool is_taken = false;
      iwdp_iport_t *ipp;
      for (ipp = iports; *ipp; ipp++) {
        if ((*ipp)->port == p) {
          is_taken = true;
          break;
        }
      }
      if (!is_taken && p != port) {
        s_fd = self->listen(self, p);
        if (s_fd > 0) {
          port = p;
          break;
        }
      }
    }
    free(iports);
  }
  if (s_fd < 0) {
    return self->on_error(self, "Unable to bind %s on port %d-%d",
        (device_id ? device_id : "\"devices list\""),
        min_port, max_port);
  }
  if (self->add_fd(self, s_fd, NULL, iport, true)) {
    return self->on_error(self, "add_fd s_fd=%d failed", s_fd);
  }
  iport->s_fd = s_fd;
  iport->port = port;
  if (!device_id) {
    iwdp_log_connect(iport);
  }
  return DL_SUCCESS;
}

iwdp_status iwdp_start(iwdp_t self) {
  iwdp_private_t my = self->private_state;
  if (my->idl) {
    return self->on_error(self, "Already started?");
  }

  if (iwdp_listen(self, NULL)) {
    // Okay, keep going
  }

  iwdp_idl_t idl = iwdp_idl_new();
  idl->self = self;

  int dl_fd = self->subscribe(self);
  if (dl_fd < 0) {  // usbmuxd isn't running
    return self->on_error(self, "No device found, is it plugged in?");
  }
  idl->dl_fd = dl_fd;

  if (self->add_fd(self, dl_fd, NULL, idl, false)) {
    return self->on_error(self, "add_fd failed");
  }

  dl_t dl = idl->dl;
  if (dl->start(dl)) {
    return self->on_error(self, "Unable to start device_listener");
  }

  // TODO add iOS simulator listener
  // for now we'll fake a callback
  dl->on_attach(dl, "SIMULATOR", -1);

  return IWDP_SUCCESS;
}

dl_status iwdp_send_to_dl(dl_t dl, const char *buf, size_t length) {
  iwdp_idl_t idl = (iwdp_idl_t)dl->state;
  iwdp_t self = idl->self;
  int dl_fd = idl->dl_fd;
  return self->send(self, dl_fd, buf, length);
}

dl_status iwdp_on_attach(dl_t dl, const char *device_id, int device_num) {
  iwdp_t self = ((iwdp_idl_t)dl->state)->self;
  if (!device_id) {
    return self->on_error(self, "Null device_id");
  }

  if (iwdp_listen(self, device_id)) {
    // Couldn't bind browser port, or we're simply ignoring this device
    return DL_SUCCESS;
  }
  iwdp_private_t my = self->private_state;

  // Return "success" on most errors, otherwise we'll kill our
  // device_listener and, via iwdp_idl_close, all our iports!

  ht_t iport_ht = my->device_id_to_iport;
  iwdp_iport_t iport = (iwdp_iport_t)ht_get_value(iport_ht, device_id);
  if (!iport) {
    return self->on_error(self, "Internal error: !iport %s", device_id);
  }
  if (iport->iwi) {
    self->on_error(self, "%s already on :%d", device_id, iport->port);
    return DL_SUCCESS;
  }
  char *device_name = iport->device_name;
  int device_os_version = 0;

  // connect to inspector
  int wi_fd;
  void *ssl_session = NULL;
  bool is_sim = !strcmp(device_id, "SIMULATOR");
  if (is_sim) {
    // TODO launch webinspectord
    // For now we'll assume Safari starts it for us.
    //
    // `launchctl list` shows:
    //   com.apple.iPhoneSimulator:com.apple.webinspectord
    // so the launch is probably something like:
    //   xpc_connection_create[_mach_service](...webinspectord, ...)?
    wi_fd = self->connect(self, my->sim_wi_socket_addr);
  } else {
    wi_fd = self->attach(self, device_id, NULL,
      (device_name ? NULL : &device_name), &device_os_version, &ssl_session);
  }
  if (wi_fd < 0) {
    self->remove_fd(self, iport->s_fd);
    if (!is_sim) {
      self->on_error(self, "Unable to attach %s inspector", device_id);
    }
    return DL_SUCCESS;
  }
  iport->device_name = (device_name ? device_name : strdup(device_id));
  iport->device_os_version = device_os_version;
  iwdp_iwi_t iwi = iwdp_iwi_new(!is_sim && device_os_version < 0xb0000,
      self->is_debug);
  iwi->iport = iport;
  iport->iwi = iwi;
  if (self->add_fd(self, wi_fd, ssl_session, iwi, false)) {
    self->remove_fd(self, iport->s_fd);
    return self->on_error(self, "add_fd wi_fd=%d failed", wi_fd);
  }
  iwi->wi_fd = wi_fd;

  // start inspector
  rpc_new_uuid(&iwi->connection_id);
  rpc_t rpc = iwi->rpc;
  if (rpc->send_reportIdentifier(rpc, iwi->connection_id)) {
    self->remove_fd(self, iport->s_fd);
    self->on_error(self, "Unable to report to inspector %s",
        device_id);
    return DL_SUCCESS;
  }

  iport->is_sticky = true;
  return DL_SUCCESS;
}

dl_status iwdp_on_detach(dl_t dl, const char *device_id, int device_num) {
  iwdp_idl_t idl = (iwdp_idl_t)dl->state;
  iwdp_t self = idl->self;
  iwdp_private_t my = self->private_state;
  iwdp_iport_t iport = (iwdp_iport_t)ht_get_value(my->device_id_to_iport,
      device_id);
  if (iport && iport->s_fd > 0) {
    self->remove_fd(self, iport->s_fd);
  }
  return IWDP_SUCCESS;
}

//
// socket I/O
//

iwdp_status iwdp_iport_accept(iwdp_t self, iwdp_iport_t iport, int ws_fd,
    iwdp_iws_t *to_iws) {
  iwdp_iws_t iws = iwdp_iws_new(self->is_debug);
  iws->iport = iport;
  iws->ws_fd = ws_fd;
  rpc_new_uuid(&iws->ws_id);
  ht_put(iport->ws_id_to_iws, iws->ws_id, iws);
  *to_iws = iws;
  return IWDP_SUCCESS;
}

iwdp_status iwdp_on_accept(iwdp_t self, int s_fd, void *value,
    int fd, void **to_value) {
  int type = ((iwdp_type_t)value)->type;
  if (type == TYPE_IPORT) {
    return iwdp_iport_accept(self, (iwdp_iport_t)value, fd,
        (iwdp_iws_t*)to_value);
  } else {
    return self->on_error(self, "Unexpected accept type %d", type);
  }
}

iwdp_status iwdp_on_recv(iwdp_t self, int fd, void *value,
    const char *buf, ssize_t length) {
  int type = ((iwdp_type_t)value)->type;
  switch (type) {
    case TYPE_IDL:
      {
        dl_t dl = ((iwdp_idl_t)value)->dl;
        return dl->on_recv(dl, buf, length);
      }
    case TYPE_IWI:
      {
        wi_t wi = ((iwdp_iwi_t)value)->wi;
        return wi->on_recv(wi, buf, length);
      }
    case TYPE_IWS:
      {
        ws_t ws = ((iwdp_iws_t)value)->ws;
        return ws->on_recv(ws, buf, length);
      }
    case TYPE_IFS:
      {
        int ws_fd = ((iwdp_ifs_t)value)->iws->ws_fd;
        iwdp_status ret = self->send(self, ws_fd, buf, length);
        if (ret) {
          self->remove_fd(self, ws_fd);
        }
        return ret;
      }
    default:
      return self->on_error(self, "Unexpected recv type %d", type);
  }
}

iwdp_status iwdp_iport_close(iwdp_t self, iwdp_iport_t iport) {
  iwdp_private_t my = self->private_state;
  // check pointer to this iport
  const char *device_id = iport->device_id;
  ht_t iport_ht = my->device_id_to_iport;
  iwdp_iport_t old_iport = (iwdp_iport_t)ht_get_value(iport_ht, device_id);
  if (old_iport != iport) {
    return self->on_error(self, "Internal iport mismatch?");
  }
  // close clients
  iwdp_iws_t *iwss = (iwdp_iws_t *)ht_values(iport->ws_id_to_iws);
  iwdp_iws_t *iws;
  for (iws = iwss; *iws; iws++) {
    if ((*iws)->ws_fd > 0) {
      self->remove_fd(self, (*iws)->ws_fd);
    }
  }
  free(iwss);
  ht_clear(iport->ws_id_to_iws);
  // close iwi
  iwdp_iwi_t iwi = iport->iwi;
  if (iwi) {
    iwdp_log_disconnect(iport);
    iwi->iport = NULL;
    iport->iwi = NULL;
    if (iwi->wi_fd > 0) {
      self->remove_fd(self, iwi->wi_fd);
    }
  }
  if (iport->is_sticky) {
    // keep iport so we can restore the port if this device is reattached
    iport->s_fd = -1;
  } else {
    ht_remove(iport_ht, device_id);
    iwdp_iport_free(iport);
  }
  return IWDP_SUCCESS;
}

iwdp_status iwdp_iws_close(iwdp_t self, iwdp_iws_t iws) {
  // clear pointer to this iws
  iwdp_ipage_t ipage = iws->ipage;