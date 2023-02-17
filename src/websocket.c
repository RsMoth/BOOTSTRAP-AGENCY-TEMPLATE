
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
  free(text);
  text = NULL;

  // base64 encode
  size_t length = 0;
  base64_encode(NULL, &length, NULL, 20);
  char *ret = (char *)malloc(length);
  if (!ret) {
    return NULL;
  }
  if (base64_encode((unsigned char *)ret, &length, hash, 20)) {
    free(ret);
    return NULL;
  }

  return ret;
}

void ws_random_buf(char *buf, size_t len) {
#ifdef __MACH__
  arc4random_buf(buf, len);
#else
  static bool seeded = false;
  if (!seeded) {
    seeded = true;
    // could fread from /dev/random
    srand(time(NULL));
  }
  size_t i;
  for (i = 0; i < len; i++) {
    buf[i] = (char)rand();
  }
#endif
}

ws_status ws_send_connect(ws_t self,
    const char *resource, const char *protocol,
    const char *host, const char *origin) {
  ws_private_t my = self->private_state;

  if (!resource) {
    return self->on_error(self, "Null arg");
  }

  char sec_ukey[20];
  ws_random_buf(sec_ukey, 20);
  size_t key_length = 0;
  base64_encode(NULL, &key_length, NULL, 20);
  char *sec_key = (char *)malloc(key_length);
  if (!sec_key) {
    return self->on_error(self, "Out of memory");
  }
  if (base64_encode((unsigned char *)sec_key, &key_length,
      (const unsigned char *)sec_ukey, 20)) {
    free(sec_key);
    return self->on_error(self, "base64_encode failed");
  }

  size_t needed = (1024 + strlen(resource) + strlen(sec_key) +
      (protocol ? strlen(protocol) : 0) +
      (host ? strlen(host) : 0) +
      (origin ? strlen(origin) : 0));
  cb_clear(my->out);
  if (cb_ensure_capacity(my->out, needed)) {
    return self->on_error(self, "Output %zd exceeds buffer capacity",
        needed);
  }
  char *out_tail = my->out->tail;

  out_tail += sprintf(out_tail,
      "GET %s HTTP/1.1\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Version: 13\r\n"
      "Sec-WebSocket-Key: %s\r\n", resource, sec_key);
  if (protocol) {
    out_tail += sprintf(out_tail, "Sec-WebSocket-Protocol: %s\r\n",
        protocol);
  }
  if (host) {
    out_tail += sprintf(out_tail, "Host: %s\r\n", host);
  }
  if (origin) {
    out_tail += sprintf(out_tail, "Origin: %s\r\n", origin);
  }
  out_tail += sprintf(out_tail, "\r\n");

  size_t out_length = out_tail - my->out->tail;
  ws_on_debug(self, "ws.send_connect", my->out->tail, out_length);
  ws_status ret = self->send_data(self, my->out->tail, out_length);
  my->out->tail = out_tail;