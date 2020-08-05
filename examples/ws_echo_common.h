
// Google BSD license https://developers.google.com/google-bsd-license
// Copyright 2012 Google Inc. wrightt@google.com

//
// A minimal websocket "echo" server
//

#ifndef WS_ECHO_COMMON_H
#define	WS_ECHO_COMMON_H

#ifdef	__cplusplus
extern "C" {
#endif


#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "ios-webkit-debug-proxy/websocket.h"

struct my_struct {
  int fd;
  int port;
  ws_t ws;
};
typedef struct my_struct *my_t;
my_t my_new(int fd, int port);
void my_free(my_t my);


ws_status send_data(ws_t ws, const char *data, size_t length);

ws_status on_http_request(ws_t ws,
        const char *method, const char *resource, const char *version,
        const char *host, const char *headers, size_t headers_length,