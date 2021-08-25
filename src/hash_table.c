
// Google BSD license https://developers.google.com/google-bsd-license
// Copyright 2012 Google Inc. wrightt@google.com

//
// A basic hash table, could be easily enhanced...
//

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "hash_table.h"

// constant for now, but we could easily resize & rehash
#define NUM_BUCKETS 3

struct ht_entry_struct {
  intptr_t hc;
  void *key;
  void *value;
  ht_entry_t next;
};


intptr_t on_strhash(ht_t ht, const void *key) {
  int hc = 0;
  char *s = (char *)key;
  if (s) {
    int ch;
    while ((ch = *s++)) {
      hc = ((hc << 5) + hc) ^ ch;
    }
  }
  return hc;
}
intptr_t on_strcmp(ht_t ht, const void *key1, const void *key2) {
  if (key1 == key2 || !key1 || !key2) {
    return (key1 == key2 ? 0 : key1 ? -1 : 1);
  }
  return strcmp(key1, key2);
}

void ht_clear(ht_t self) {
  size_t i;
  for (i = 0; i < self->num_buckets; i++) {
    ht_entry_t curr = self->buckets[i];
    while (curr) {
      ht_entry_t next = curr->next;
      memset(curr, 0, sizeof(struct ht_entry_struct));
      free(curr);
      self->num_keys--;
      curr = next;
    }
    self->buckets[i] = NULL;
  }
}

void ht_free(ht_t self) {
  if (self) {
    ht_clear(self);
    free(self->buckets);
    memset(self, 0, sizeof(struct ht_struct));
    free(self);
  }
}

ht_t ht_new(enum ht_key_type type) {
  ht_t self = (ht_t)malloc(sizeof(struct ht_struct));
  if (self) {
    memset(self, 0, sizeof(struct ht_struct));
    self->num_buckets = NUM_BUCKETS;
    self->buckets = (ht_entry_t *)calloc(self->num_buckets,
        sizeof(ht_entry_t));
    if (type == HT_STRING_KEYS) {
      self->on_hash = on_strhash;
      self->on_cmp = on_strcmp;
    }
  }
  return self;
}

size_t ht_size(ht_t self) {
  return self->num_keys;
}

void ht_find(ht_t self, const void *key, intptr_t *to_hc,
    ht_entry_t **to_head, ht_entry_t *to_prev, ht_entry_t *to_curr) {
  intptr_t hc = (self->on_hash ? self->on_hash(self, key) : (intptr_t)key);
  ht_entry_t *head = self->buckets + (hc % self->num_buckets);
  ht_entry_t prev = NULL;
  ht_entry_t curr = *head;
  for (; curr && !(curr->hc == hc &&
      (self->on_cmp ? !self->on_cmp(self, curr->key, key) :
       curr->key == key));
      prev = curr, curr = curr->next) {
  }