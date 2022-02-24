
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