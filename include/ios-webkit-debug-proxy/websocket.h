
// Google BSD license https://developers.google.com/google-bsd-license
// Copyright 2012 Google Inc. wrightt@google.com

#ifndef WEBSOCKET_H
#define	WEBSOCKET_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdint.h>


typedef uint8_t ws_opcode;
#define OPCODE_CONTINUATION  0x0
#define OPCODE_TEXT          0x1
#define OPCODE_BINARY        0x2
#define OPCODE_CLOSE         0x8
#define OPCODE_PING          0x9
#define OPCODE_PONG          0xA

typedef uint16_t ws_close;
#define CLOSE_NORMAL         1000
#define CLOSE_GOING_AWAY     1001
#define CLOSE_PROTOCOL_ERROR 1002
#define CLOSE_BAD_DATA_TYPE  1003
#define CLOSE_INVALID_DATA   1007
#define CLOSE_POLICY_ERROR   1008
#define CLOSE_SIZE_ERROR     1009
#define CLOSE_NO_EXTENSION   1010
#define CLOSE_SERVER_ERROR   1011

typedef uint8_t ws_status;
#define WS_ERROR 1
#define WS_SUCCESS 0


struct ws_struct;
typedef struct ws_struct *ws_t;
ws_t ws_new();
void ws_free(ws_t self);

struct ws_private;
typedef struct ws_private *ws_private_t;

struct ws_struct {

  //
  // Use these APIs: