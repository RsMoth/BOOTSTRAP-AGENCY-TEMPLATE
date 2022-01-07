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
  pc_entry_t tail;
};

pc_t pc_new() {
  pc_t self = malloc(sizeof(struct pc_struct));
  if (self) {
    memset(self, 0, sizeof(struct pc_struct));
  }
  return self;
}

void pc_clear(pc_t self) {
  if (self) {
    pc_entry_t e = self->head;
    while (e) {
      pc_entry_t next = e->next;
      memset(e, 0, sizeof(struct pc_entry_struct));
      free(e);
      e = next;
    }
    self->head = NULL;
    self->tail = NULL;
  }
}

void pc_free(pc_t self) {
  if (self) {
    pc_clear(self);
    free(self->groups);
    if (self->re) {
      regfree(self->re);
    }
    memset(self, 0, sizeof(struct pc_struct));
    free(self);
  }
}

void pc_add(pc_t self, const char *device_id, int min_port, int max_port) {
  pc_entry_t e = malloc(sizeof(struct pc_entry_struct));
  e->device_id = device_id;
  e->min_port = min_port;
  e->max_port = max_port;
  e->next = NULL;
  if (self->tail) {
    self->tail->next = e;
  } else {
    self->head = e;
  }
  self->tail = e;
}

int pc_parse