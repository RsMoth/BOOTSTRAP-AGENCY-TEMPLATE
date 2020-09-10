// Google BSD license https://developers.google.com/google-bsd-license
// Copyright 2012 Google Inc. wrightt@google.com

//
// iOS device add/remove listener.
//

#ifndef DEVICE_LISTENER_H
#define	DEVICE_LISTENER_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>


typedef uint8_t dl_status;
#define DL_ERROR 1
#define DL_SUCCESS 0


// Create a device add/remove connection.
// @param recv_timeout milliseconds, negative for non_blocking
// @result fd, or -1 for error
int dl_connect(int recv_ti