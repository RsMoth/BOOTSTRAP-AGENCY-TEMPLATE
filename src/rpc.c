
// Google BSD license https://developers.google.com/google-bsd-license
// Copyright 2014 Google Inc. wrightt@google.com

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
#include <time.h>

#ifdef __MACH__
#include <uuid/uuid.h>
#endif

#include "rpc.h"


rpc_status rpc_parse_app(const plist_t node, rpc_app_t *app);
void rpc_free_app(rpc_app_t app);

rpc_status rpc_parse_apps(const plist_t node, rpc_app_t **to_apps);
void rpc_free_apps(rpc_app_t *apps);

rpc_status rpc_parse_pages(const plist_t node, rpc_page_t **to_pages);
void rpc_free_pages(rpc_page_t *pages);

rpc_status rpc_args_to_xml(rpc_t self,
    const void *args_obj, char **to_xml, bool should_trim);

rpc_status rpc_dict_get_required_string(const plist_t node, const char *key,
    char **to_value);
rpc_status rpc_dict_get_optional_string(const plist_t node, const char *key,
    char **to_value);
rpc_status rpc_dict_get_required_bool(const plist_t node, const char *key,
    bool *to_value);
rpc_status rpc_dict_get_optional_bool(const plist_t node, const char *key,
    bool *to_value);
rpc_status rpc_dict_get_required_uint(const plist_t node, const char *key,
    uint32_t *to_value);
rpc_status rpc_dict_get_required_data(const plist_t node, const char *key,
    char **to_value, size_t *to_length);

//
// UUID
//

rpc_status rpc_new_uuid(char **to_uuid) {
  if (!to_uuid) {
    return RPC_ERROR;
  }
#ifdef __MACH__
  *to_uuid = (char *)malloc(37);
  uuid_t uuid;
  uuid_generate(uuid);
  uuid_unparse_upper(uuid, *to_uuid);
#else
  // see stackoverflow.com/questions/2174768/clinuxgenerating-uuids-in-linux
  static bool seeded = false;
  if (!seeded) {
    seeded = true;
    srand(time(NULL));
  }
  if (asprintf(to_uuid, "%x%x-%x-%x-%x-%x%x%x",
      rand(), rand(), rand(),
      ((rand() & 0x0fff) | 0x4000),
      rand() % 0x3fff + 0x8000,
      rand(), rand(), rand()) < 0) {
    return RPC_ERROR;  // asprintf failed
  }
#endif
  return RPC_SUCCESS;
}


//
// SEND
//

rpc_status rpc_on_error(rpc_t self, const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  fprintf(stderr, "\n");
  va_end(args);
  return RPC_ERROR;
}

plist_t rpc_new_args(const char *connection_id) {
  plist_t ret = plist_new_dict();
  if (connection_id) {
    plist_dict_set_item(ret, "WIRConnectionIdentifierKey",
        plist_new_string(connection_id));
  }
  return ret;
}

/*
   WIRFinalMessageKey
   __selector
   __argument
 */
rpc_status rpc_send_msg(rpc_t self, const char *selector, plist_t args) {
  if (!selector || !args) {
    return RPC_ERROR;
  }
  plist_t rpc_dict = plist_new_dict();
  plist_dict_set_item(rpc_dict, "__selector",
      plist_new_string(selector));
  plist_dict_set_item(rpc_dict, "__argument", plist_copy(args));
  rpc_status ret = self->send_plist(self, rpc_dict);
  plist_free(rpc_dict);
  return ret;
}

/*
_rpc_reportIdentifier:
<key>WIRConnectionIdentifierKey</key>
<string>4B2550E4-13D6-4902-A48E-B45D5B23215B</string>
 */
rpc_status rpc_send_reportIdentifier(rpc_t self, const char *connection_id) {
  if (!connection_id) {
    return RPC_ERROR;
  }
  const char *selector = "_rpc_reportIdentifier:";
  plist_t args = rpc_new_args(connection_id);
  rpc_status ret = rpc_send_msg(self, selector, args);
  plist_free(args);
  return ret;
}

/*
_rpc_getConnectedApplications:
<key>WIRConnectionIdentifierKey</key>
<string>4B2550E4-13D6-4902-A48E-B45D5B23215B</string>
 */
rpc_status rpc_send_getConnectedApplications(rpc_t self,
    const char *connection_id) {
  if (!connection_id) {
    return RPC_ERROR;
  }
  const char *selector = "_rpc_getConnectedApplications:";
  plist_t args = rpc_new_args(connection_id);
  rpc_status ret = rpc_send_msg(self, selector, args);
  plist_free(args);
  return ret;
}

/*
_rpc_forwardGetListing:
<key>WIRApplicationIdentifierKey</key>
<string>com.apple.mobilesafari</string>
<key>WIRConnectionIdentifierKey</key>
<string>4B2550E4-13D6-4902-A48E-B45D5B23215B</string>
 */
rpc_status rpc_send_forwardGetListing(rpc_t self, const char *connection_id,
    const char *app_id) {
  if (!connection_id || !app_id) {
    return RPC_ERROR;
  }
  const char *selector = "_rpc_forwardGetListing:";
  plist_t args = rpc_new_args(connection_id);
  plist_dict_set_item(args, "WIRApplicationIdentifierKey",
      plist_new_string(app_id));
  rpc_status ret = rpc_send_msg(self, selector, args);
  plist_free(args);
  return ret;
}

/*
_rpc_forwardIndicateWebView:
<key>WIRApplicationIdentifierKey</key> <string>com.apple.mobilesafari</string>
<key>WIRConnectionIdentifierKey</key>
<string>4B2550E4-13D6-4902-A48E-B45D5B23215B</string>
<key>WIRIndicateEnabledKey</key>  <true/>
<key>WIRPageIdentifierKey</key>   <integer>1</integer>
 */
rpc_status rpc_send_forwardIndicateWebView(rpc_t self, const char *connection_id,
    const char *app_id, uint32_t page_id, bool is_enabled) {
  if (!connection_id || !app_id) {
    return RPC_ERROR;
  }
  const char *selector = "_rpc_forwardIndicateWebView:";
  plist_t args = rpc_new_args(connection_id);
  plist_dict_set_item(args, "WIRApplicationIdentifierKey",
      plist_new_string(app_id));
  plist_dict_set_item(args, "WIRPageIdentifierKey",
      plist_new_uint(page_id));
  plist_dict_set_item(args, "WIRIndicateEnabledKey",
      plist_new_bool(is_enabled));
  rpc_status ret = rpc_send_msg(self, selector, args);
  plist_free(args);
  return ret;
}

/*
_rpc_forwardSocketSetup:
<key>WIRApplicationIdentifierKey</key> <string>com.apple.mobilesafari</string>
<key>WIRConnectionIdentifierKey</key>
<string>4B2550E4-13D6-4902-A48E-B45D5B23215B</string>
<key>WIRPageIdentifierKey</key>   <integer>1</integer>
<key>WIRSenderKey</key>
<string>C1EAD225-D6BC-44B9-9089-2D7CC2D2204C</string>
 */
rpc_status rpc_send_forwardSocketSetup(rpc_t self, const char *connection_id,
    const char *app_id, uint32_t page_id, const char *sender_id) {
  if (!connection_id || !app_id || !sender_id) {
    return RPC_ERROR;
  }
  const char *selector = "_rpc_forwardSocketSetup:";
  plist_t args = rpc_new_args(connection_id);
  plist_dict_set_item(args, "WIRApplicationIdentifierKey",
      plist_new_string(app_id));
  plist_dict_set_item(args, "WIRAutomaticallyPause",
      plist_new_bool(false));
  plist_dict_set_item(args, "WIRPageIdentifierKey",
      plist_new_uint(page_id));
  plist_dict_set_item(args, "WIRSenderKey",
      plist_new_string(sender_id));
  rpc_status ret = rpc_send_msg(self, selector, args);
  plist_free(args);
  return ret;
}

/*
_rpc_forwardSocketData:
<key>WIRApplicationIdentifierKey</key>
<string>com.apple.mobilesafari</string>
<key>WIRConnectionIdentifierKey</key>
<string>4B2550E4-13D6-4902-A48E-B45D5B23215B</string>
<key>WIRPageIdentifierKey</key>
<integer>1</integer>
<key>WIRSenderKey</key>
<string>C1EAD225-D6BC-44B9-9089-2D7CC2D2204C</string>
<key>WIRSocketDataKey **data**</key>
<data>
{"method":"Debugger.causesRecompilation","id":1}
</data>
 */
rpc_status rpc_send_forwardSocketData(rpc_t self, const char *connection_id,
    const char *app_id, uint32_t page_id, const char *sender_id,
    const char *data, size_t length) {
  if (!connection_id || !app_id || !sender_id || !data) {
    return RPC_ERROR;
  }
  const char *selector = "_rpc_forwardSocketData:";
  plist_t args = rpc_new_args(connection_id);
  plist_dict_set_item(args, "WIRApplicationIdentifierKey",
      plist_new_string(app_id));
  plist_dict_set_item(args, "WIRPageIdentifierKey",
      plist_new_uint(page_id));
  plist_dict_set_item(args, "WIRSenderKey",
      plist_new_string(sender_id));
  plist_dict_set_item(args, "WIRSocketDataKey",
      plist_new_data(data, length));
  rpc_status ret = rpc_send_msg(self, selector, args);
  plist_free(args);
  return ret;
}