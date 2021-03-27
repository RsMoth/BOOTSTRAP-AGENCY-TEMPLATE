
// Google BSD license https://developers.google.com/google-bsd-license
// Copyright 2012 Google Inc. wrightt@google.com

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "char_buffer.h"

#define MIN_LENGTH 1024

cb_t cb_new() {
  cb_t self = (cb_t)malloc(sizeof(struct cb_struct));
  if (self) {
    memset(self, 0, sizeof(struct cb_struct));
  }
  return self;
}

void cb_free(cb_t self) {
  if (self) {
    if (self->begin) {
      free(self->begin);
    }
    free(self);
  }
}

void cb_clear(cb_t self) {
  self->head = self->begin;
  self->tail = self->begin;
}

int cb_ensure_capacity(cb_t self, size_t needed) {
  if (!self->begin) {
    size_t length = (needed > MIN_LENGTH ? needed : MIN_LENGTH);
    self->begin = (char *)malloc(length * sizeof(char));
    if (!self->begin) {
      perror("Unable to allocate buffer");
      return -1;
    }
    self->head = self->begin;
    self->tail = self->begin;
    self->end = self->begin + length;
    return 0;
  }
  size_t used = self->tail - self->head;
  if (!used) {
    self->head = self->begin;
    self->tail = self->begin;
  }
  size_t avail = self->end - self->tail;
  if (needed > avail) {
    size_t offset = self->head - self->begin;
    if (offset) {
      if (used) {
        memmove(self->begin, self->head, used);
      }
      self->head = self->begin;
      self->tail = self->begin + used;
      avail += offset;
    }
    if (needed > avail) {
      size_t length = self->end - self->begin;
      size_t new_length = used + needed;
      if (new_length < 1.5 * length) {
        new_length = 1.5 * length;
      }
      char *new_begin = (char*)realloc(self->begin,
          new_length * sizeof(char));
      if (!new_begin) {
        perror("Unable to resize buffer");
        return -1;
      }
      self->begin = new_begin;
      self->head = new_begin;
      self->tail = new_begin + used;
      self->end = new_begin + new_length;
    }
  }
  return 0;