
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

/*
_rpc_forwardDidClose:
?
 */
rpc_status rpc_send_forwardDidClose(rpc_t self, const char *connection_id,
    const char *app_id, uint32_t page_id, const char *sender_id) {
  if (!connection_id || !app_id || !sender_id) {
    return RPC_ERROR;
  }
  const char *selector = "_rpc_forwardDidClose:";
  plist_t args = rpc_new_args(connection_id);
  plist_dict_set_item(args, "WIRApplicationIdentifierKey",
      plist_new_string(app_id));
  plist_dict_set_item(args, "WIRPageIdentifierKey",
      plist_new_uint(page_id));
  plist_dict_set_item(args, "WIRSenderKey",
      plist_new_string(sender_id));
  rpc_status ret = rpc_send_msg(self, selector, args);
  plist_free(args);
  return ret;
}


//
// RECV
//

/*
 */
rpc_status rpc_recv_reportSetup(rpc_t self, const plist_t args) {
  if (plist_get_node_type(args) != PLIST_DICT) {
    return RPC_ERROR;
  }
  return self->on_reportSetup(self);
}

/*
   <key>WIRApplicationDictionaryKey</key>
   <dict>
   <key>com.apple.mobilesafari</key>
   <dict>
   <key>WIRApplicationIdentifierKey</key>
   <string>com.apple.mobilesafari</string>
   <key>WIRApplicationNameKey</key>    <string>Safari</string>
   <key>WIRIsApplicationProxyKey</key> <false/>
   </dict>
   </dict>
 */
rpc_status rpc_recv_reportConnectedApplicationList(rpc_t self,
    const plist_t args) {
  plist_t item = plist_dict_get_item(args, "WIRApplicationDictionaryKey");
  rpc_app_t *apps = NULL;
  rpc_status ret = rpc_parse_apps(item, &apps);
  if (!ret) {
    ret = self->on_reportConnectedApplicationList(self, apps);
    rpc_free_apps(apps);
  }
  return ret;
}

/*
 */
rpc_status rpc_recv_applicationConnected(rpc_t self, const plist_t args) {
  rpc_app_t app = NULL;
  rpc_status ret = rpc_parse_app(args, &app);
  if (!ret) {
    ret = self->on_applicationConnected(self, app);
    rpc_free_app(app);
  }
  return ret;
}

/*
 */
rpc_status rpc_recv_applicationDisconnected(rpc_t self, const plist_t args) {
  rpc_app_t app = NULL;
  rpc_status ret = rpc_parse_app(args, &app);
  if (!ret) {
    ret = self->on_applicationDisconnected(self, app);
    rpc_free_app(app);
  }
  return ret;
}

/*
_rpc_applicationSentListing:
string appId = args[“WIRApplicationIdentifierKey”]
for val in args[“WIRListingKey”].values():
string pageId = val[“WIRPageIdentifierKey”]
pageInfo.title = val[“WIRTitleKey”]
pageInfo.url = val[“WIRURLKey”]
pageInfo.connectionId = val[“WIRConnectionIdentifierKey”]
<key>WIRApplicationIdentifierKey</key>  <string>com.apple.mobilesafari</string>
<key>WIRListingKey</key>
<dict>
<key>1</key>
<dict>
<key>WIRConnectionIdentifierKey</key>
<string>4B2550E4-13D6-4902-A48E-B45D5B23215B</string>
<key>WIRPageIdentifierKey</key>   <integer>1</integer>
<key>WIRTitleKey</key>   <string>Cannot Open Page</string>
<key>WIRURLKey</key>
<string>x-appbundle:///StandardError.html</string>
</dict>
</dict>
 */
rpc_status rpc_recv_applicationSentListing(rpc_t self, const plist_t args) {
  char *app_id = NULL;
  rpc_page_t *pages = NULL;
  plist_t item = plist_dict_get_item(args, "WIRListingKey");
  rpc_status ret;
  if (!rpc_dict_get_required_string(args, "WIRApplicationIdentifierKey",
        &app_id) &&
      !rpc_parse_pages(item, &pages) &&
      !self->on_applicationSentListing(self, app_id, pages)) {
    ret = RPC_SUCCESS;
  } else {
    ret = RPC_ERROR;
  }
  free(app_id);
  rpc_free_pages(pages);
  return ret;
}

/*
_rpc_applicationSentData:
string appId = args[“WIRApplicationIdentifierKey”]
string socketKey = args[“WIRDestinationKey”]
string jsonData = args[“WIRMessageDataKey”]
<key>WIRApplicationIdentifierKey</key>
<string>com.apple.mobilesafari</string>
<key>WIRDestinationKey</key>
<string>C1EAD225-D6BC-44B9-9089-2D7CC2D2204C</string>
<key>WIRMessageDataKey</key>
<data>
{"result":{"result":true},"id":1}
</data>
 */
rpc_status rpc_recv_applicationSentData(rpc_t self, const plist_t args) {
  char *app_id = NULL;
  char *dest_id = NULL;
  char *data = NULL;
  size_t length = 0;
  rpc_status ret;
  if (!rpc_dict_get_required_string(args, "WIRApplicationIdentifierKey",
        &app_id) &&
      !rpc_dict_get_required_string(args, "WIRDestinationKey",
        &dest_id) &&
      !rpc_dict_get_required_data(args, "WIRMessageDataKey",
        &data, &length) &&
      !self->on_applicationSentData(self,
        app_id, dest_id, data, length)) {
    ret = RPC_SUCCESS;
  } else {
    ret = RPC_ERROR;
  }
  free(app_id);
  free(dest_id);
  free(data);
  return ret;
}

/*
_rpc_applicationUpdated: <dict>
<key>WIRApplicationBundleIdentifierKey</key>
<string>com.apple.WebKit.WebContent</string>
<key>WIRHostApplicationIdentifierKey</key>
<string>PID:409</string>
<key>WIRApplicationNameKey</key>
<string></string>
<key>WIRIsApplicationProxyKey</key>
<true/>
<key>WIRIsApplicationActiveKey</key>
<integer>0</integer>
<key>WIRApplicationIdentifierKey</key>
<string>PID:536</string>
</dict>

OR

<key>WIRApplicationBundleIdentifierKey</key>
<string>com.apple.mobilesafari</string>
<key>WIRApplicationNameKey</key>
<string>Safari</string>
<key>WIRIsApplicationProxyKey</key>
<false/>
<key>WIRIsApplicationActiveKey</key>
<integer>0</integer>
<key>WIRApplicationIdentifierKey</key>
<string>PID:730</string>
*/
rpc_status rpc_recv_applicationUpdated(rpc_t self, const plist_t args) {
  char *app_id = NULL;
  char *dest_id = NULL;
  rpc_status ret;
  if (!rpc_dict_get_required_string(args, "WIRHostApplicationIdentifierKey", &app_id)) {
    if (!rpc_dict_get_required_string(args, "WIRApplicationIdentifierKey", &dest_id) &&
      !self->on_applicationUpdated(self, app_id, dest_id)) {
      ret = RPC_SUCCESS;
    } else {
      ret = RPC_ERROR;
    }
  } else if (!rpc_dict_get_required_string(args, "WIRApplicationNameKey", &app_id) &&
             !rpc_dict_get_required_string(args, "WIRApplicationIdentifierKey", &dest_id) &&
             !self->on_applicationUpdated(self, app_id, dest_id)) {
    ret = RPC_SUCCESS;
  } else {
    ret = RPC_ERROR;
  }
  free(app_id);
  free(dest_id);
  return ret;
}

rpc_status rpc_recv_msg(rpc_t self, const char *selector, const plist_t args) {
  if (!selector) {
    return RPC_ERROR;
  }

  if (!strcmp(selector, "_rpc_reportSetup:")) {
    if (!rpc_recv_reportSetup(self, args)) {
      return RPC_SUCCESS;
    }
  } else if (!strcmp(selector, "_rpc_reportConnectedApplicationList:")) {
    if (!rpc_recv_reportConnectedApplicationList(self, args)) {
      return RPC_SUCCESS;
    }
  } else if (!strcmp(selector, "_rpc_applicationConnected:")) {
    if (!rpc_recv_applicationConnected(self, args)) {
      return RPC_SUCCESS;
    }
  } else if (!strcmp(selector, "_rpc_applicationDisconnected:")) {
    if (!rpc_recv_applicationDisconnected(self, args)) {
      return RPC_SUCCESS;
    }
  } else if (!strcmp(selector, "_rpc_applicationSentListing:")) {
    if (!rpc_recv_applicationSentListing(self, args)) {
      return RPC_SUCCESS;
    }
  } else if (!strcmp(selector, "_rpc_applicationSentData:")) {
    if (!rpc_recv_applicationSentData(self, args)) {
      return RPC_SUCCESS;
    }
  } else if (!strcmp(selector, "_rpc_applicationUpdated:")) {
    if (!rpc_recv_applicationUpdated(self, args)) {
      return RPC_SUCCESS;
    }
  } else if (!strcmp(selector, "_rpc_reportConnectedDriverList:") || !strcmp(selector, "_rpc_reportCurrentState:")) {
    return RPC_SUCCESS;
  }

  // invalid msg
  char *args_xml = NULL;
  rpc_args_to_xml(self, args, &args_xml, true);
  rpc_status ret = self->on_error(self, "Invalid message %s %s",
      selector, args_xml);
  free(args_xml);
  return ret;
}

rpc_status rpc_recv_plist(rpc_t self, const plist_t rpc_dict) {
  char *selector = NULL;
  plist_get_string_val(plist_dict_get_item(rpc_dict, "__selector"), &selector);
  plist_t args = plist_dict_get_item(rpc_dict, "__argument");
  return rpc_recv_msg(self, selector, args);
}

//
// STRUCTS
//

void rpc_free(rpc_t self) {
  if (self) {
    memset(self, 0, sizeof(struct rpc_struct));
    free(self);
  }
}

rpc_t rpc_new() {
  rpc_t self = (rpc_t)malloc(sizeof(struct rpc_struct));
  if (!self) {
    return NULL;
  }
  memset(self, 0, sizeof(struct rpc_struct));
  self->send_reportIdentifier = rpc_send_reportIdentifier;
  self->send_getConnectedApplications = rpc_send_getConnectedApplications;
  self->send_forwardGetListing = rpc_send_forwardGetListing;
  self->send_forwardIndicateWebView = rpc_send_forwardIndicateWebView;
  self->send_forwardSocketSetup = rpc_send_forwardSocketSetup;
  self->send_forwardSocketData = rpc_send_forwardSocketData;
  self->send_forwardDidClose = rpc_send_forwardDidClose;
  self->recv_plist = rpc_recv_plist;
  self->on_error = rpc_on_error;
  return self;
}

rpc_app_t rpc_new_app() {
  rpc_app_t app = (rpc_app_t)malloc(sizeof(struct rpc_app_struct));
  if (app) {
    memset(app, 0, sizeof(struct rpc_app_struct));
  }
  return app;
}

void rpc_free_app(rpc_app_t app) {
  if (app) {
    free(app->app_id);
    free(app->app_name);
    memset(app, 0, sizeof(struct rpc_app_struct));
    free(app);
  }
}

rpc_status rpc_copy_app(rpc_app_t app, rpc_app_t *to_app) {
  rpc_app_t new_app = (to_app ? rpc_new_app() : NULL);
  if (!new_app) {
    return RPC_ERROR;
  }

  new_app->app_id = strdup(app->app_id);
  new_app->app_name = strdup(app->app_name);
  new_app->is_proxy = app->is_proxy;
  *to_app = new_app;
  return RPC_SUCCESS;
}

rpc_status rpc_parse_app(const plist_t node, rpc_app_t *to_app) {
  rpc_app_t app = (to_app ? rpc_new_app() : NULL);
  if (!app ||
      rpc_dict_get_required_string(node, "WIRApplicationIdentifierKey",
        &app->app_id) ||
      rpc_dict_get_optional_string(node, "WIRApplicationNameKey",
        &app->app_name) ||
      rpc_dict_get_optional_bool(node, "WIRIsApplicationProxyKey",
        &app->is_proxy)) {
    rpc_free_app(app);
    if (to_app) {
      *to_app = NULL;
    }
    return RPC_ERROR;
  }
  *to_app = app;
  return RPC_SUCCESS;
}

void rpc_free_apps(rpc_app_t *apps) {
  if (apps) {
    rpc_app_t *a = apps;
    while (*a) {
      rpc_free_app(*a++);
    }
    free(apps);
  }
}

rpc_status rpc_parse_apps(const plist_t node, rpc_app_t **to_apps) {
  if (!to_apps) {
    return RPC_ERROR;