// Google BSD license https://developers.google.com/google-bsd-license
// Copyright 2012 Google Inc. wrightt@google.com

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef HAVE_REGEX_H
#include <pcre.h>
#include <pcreposix.h>
#else
#include <regex.h>
#endif

#include "port_config.h"
#include "strndup.h"
#include "getline.h"


struct pc_entry_struct;
typedef struct pc_entry_struct *pc_entry_t;

struct pc_entry_struct {
  const char *device_id;
  int min_port;
  int max_port;

  // we need a list of these, so put the link here
  pc_entry_t next;
};

struct pc_struct {
  regex_t *re;
  regmatch_t *groups;
  pc_entry_t head;