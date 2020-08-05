
// Google BSD license https://developers.google.com/google-bsd-license
// Copyright 2012 Google Inc. wrightt@google.com

//
// A minimal websocket "echo" server
//

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

#include "ws_echo_common.h"

// websocket callbacks:

ws_status send_data(ws_t ws, const char *data, size_t length) {
  int fd = ((my_t)ws->state)->fd;
  ssize_t sent_bytes = send(fd, (void*)data, length, 0);
  return (sent_bytes == length ? WS_SUCCESS : WS_ERROR);
}

char *create_root_response(int port, int count) {
  char *html = NULL;
  if (asprintf(&html,
      "<html><head><script type=\"text/javascript\">\n"
      "function WebSocketTest() {\n"
      "  if (\"WebSocket\" in window) {\n"
      "    var ws = new WebSocket(\"ws://localhost:%d/\");\n"
      "    var count = %d;\n"
      "    ws.onopen = function() {\n"
      "      alert(\"Sending \"+count);\n"