
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
