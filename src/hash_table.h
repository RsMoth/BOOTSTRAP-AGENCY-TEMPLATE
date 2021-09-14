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

ht_t ht_new(enum ht