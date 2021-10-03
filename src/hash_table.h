// Google BSD license https://developers.google.com/google-bsd-license
// Copyright 2012 Google Inc. wrightt@google.com

//
// A generic hash table implementation
//

#ifndef HASH_TABLE_H
#define	HASH_TABLE_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdint.h>

// cast int to void*
#define HT_KEY(i) ((void *)(intptr_t)i)
#define HT_VALUE(i) HT_KEY(i)

enum ht_key_type {
  HT_INT_KEYS,
  HT_STRING_KEYS
};

struct ht_entry_struct;
typedef struct ht_entry_struct *ht_entry_t;

struct ht_struct;
typedef struct ht_struct *ht_t;

ht_t ht_new(enum ht_key_type type);

// note: doesn't free keys or values!
void ht_clear(ht_t self);

void ht_free(ht_t self);

// @result Returns number of keys
size_t ht_size(ht_t self);

void *ht_get_key(ht_t self, const void *key);
void *ht_get_value(ht_t self, const void *key);

void *ht_remove(ht_t self, const void *key);

void *ht_put(ht_t self, void *key, void *value);

// @result Returns a dynamically-allocated array of le