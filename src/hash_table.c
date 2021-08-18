
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