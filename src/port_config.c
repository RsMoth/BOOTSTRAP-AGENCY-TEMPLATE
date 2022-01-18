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

int pc_parse(pc_t self, const char *line, size_t len,
    char **to_device_id, int *to_min_port, int *to_max_port) {
  if (!self->re) {
    self->re = malloc(sizeof(regex_t));
    if (regcomp(self->re,
          "^[ \t]*"
          "(([a-fA-F0-9-]{25,}|\\*|null)[ \t]*:?|:)"
          "[ \t]*(-?[0-9]+)"
          "([ \t]*-[ \t]*([0-9]+))?"
          "[ \t]*$", REG_EXTENDED | REG_ICASE)) {
      perror("Internal error: bad regex?");
      return -1;
    }
    size_t ngroups = self->re->re_nsub + 1;
    self->groups = calloc(ngroups, sizeof(regmatch_t));
  }
  size_t ngroups = self->re->re_nsub + 1;
  regmatch_t *groups = self->groups;
  char *line2 = calloc(len+1, sizeof(char));
  memcpy(line2, line, len);
  int is_not_match = regexec(self->re, line2, ngroups, groups, 0);
  free(line2);
  if (is_not_match) {
    return -1;
  }
  char *device_id;
  if (groups[2].rm_so >= 0) {
    size_t len = groups[2].rm_eo - groups[2].rm_so;
    if (strncasecmp("null", line + groups[2].rm_so, len)) {
      device_id = strndup(line +