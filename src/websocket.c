
// Google BSD license https://developers.google.com/google-bsd-license
// Copyright 2012 Google Inc. wrightt@google.com

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "websocket.h"
#include "char_buffer.h"

#include "base64.h"
#include "sha1.h"

#include "validate_utf8.h"
#include "strndup.h"
#include "strcasestr.h"

typedef int8_t ws_state;
#define STATE_ERROR 1
#define STATE_READ_HTTP_REQUEST 2
#define STATE_READ_HTTP_HEADERS 3
#define STATE_KEEP_ALIVE 4
#define STATE_READ_FRAME_LENGTH 5
#define STATE_READ_FRAME 6
#define STATE_CLOSED 7


struct ws_private {
  ws_state state;

  cb_t in;
  cb_t out;
  cb_t data;

  char *method;
  char *resource;
  char *http_version;
  char *req_host;

  char *protocol;
  int version;
  char *sec_key;
  bool is_websocket;

  char *sec_answer;

  size_t needed_length;
  size_t frame_length;

  uint8_t continued_opcode;
  bool sent_close;
};


ws_status ws_on_error(ws_t self, const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  fprintf(stderr, "\n");
  va_end(args);
  return WS_ERROR;
}

ws_status ws_on_debug(ws_t self, const char *message,
    const char *buf, size_t length) {
  //ws_private_t my = self->private_state;
  if (self->is_debug && *self->is_debug) {
    char *text;
    cb_asprint(&text, buf, length, 80, 50);
    printf("%s[%zd]:\n%s\n", message, length, text);
    free(text);
  }
  return WS_SUCCESS;
}

//
// SEND
//

static char *ws_compute_answer(const char *sec_key) {
  if (!sec_key) {
    return NULL;
  }

  // concat sec_key + magic
  static char *MAGIC = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  size_t text_length = (strlen(sec_key) + strlen(MAGIC) + 1);
  char *text = (char *)malloc(text_length * sizeof(char));
  if (!text) {
    return NULL;
  }
  sprintf(text, "%s%s", sec_key, MAGIC);

  // SHA-1 hash
  unsigned char hash[20];
  sha1_context ctx;
  sha1_starts(&ctx);
  sha1_update(&ctx, (const unsigned char *)text, text_length-1);
  sha1_finish(&ctx, hash);