
// Google BSD license https://developers.google.com/google-bsd-license
// Copyright 2012 Google Inc. wrightt@google.com

//
// A generic select-based socket manager.
//

#ifndef SOCKET_SELECTOR_H
#define	SOCKET_SELECTOR_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

// Bind a server port, return the file descriptor (or -1 for error).
int sm_listen(int port);

// Connect to a server, return the file descriptor (or -1 for error).
int sm_connect(const char *socket_addr);


typedef uint8_t sm_status;
#define SM_ERROR 1
#define SM_SUCCESS 0


struct sm_private;
typedef struct sm_private *sm_private_t;

struct sm_struct;
typedef struct sm_struct *sm_t;
sm_t sm_new(size_t buffer_length);
void sm_free(sm_t self);

struct sm_struct {

  // Call these APIs:

  // @param value a value to associate with this fd, which will be passed
  // in future on_accept/on_recv/on_close callbacks.
  sm_status (*add_fd)(sm_t self, int fd, void *ssl_session, void *value, bool
      is_server);

  sm_status (*remove_fd)(sm_t self, int fd);
