
// Google BSD license https://developers.google.com/google-bsd-license
// Copyright 2012 Google Inc. wrightt@google.com

//
// iOS WebKit Remote Debugging Protocol Proxy
//

#ifndef IOS_WEBKIT_DEBUG_PROXY_H
#define	IOS_WEBKIT_DEBUG_PROXY_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

typedef uint8_t iwdp_status;
#define IWDP_ERROR 1
#define IWDP_SUCCESS 0


struct iwdp_private;
typedef struct iwdp_private *iwdp_private_t;

struct iwdp_struct;
typedef struct iwdp_struct *iwdp_t;
iwdp_t iwdp_new(const char* frontend, const char* sim_wi_socket_addr);
void iwdp_free(iwdp_t self);

struct iwdp_struct {

  // Use these APIs:

  // Start the proxy.
  iwdp_status (*start)(iwdp_t self);

  // Accept new client.
  // @param s_fd server fd from our add_fd call
  // @param s_value value from our add_fd call
  // @param fd
  // @param to_value will be set, must be passed to subsequent on_recv and
  //     on_close calls.
  iwdp_status (*on_accept)(iwdp_t self, int s_fd, void *s_value,
                           int fd, void **to_value);

  // Receive bytes from fd.
  // @param value the *to_value set by our on_accept or add_fd
  iwdp_status (*on_recv)(iwdp_t self, int fd, void *value,
                         const char *buf, ssize_t length);

  iwdp_status (*on_close)(iwdp_t self, int fd, void *value,
                          bool is_server);

  void *state;
  bool *is_debug;


  // Provide these callbacks:

  // Subscribe to device add/remove callbacks.
  // @result fd, or -1 for error
  int (*subscribe)(iwdp_t iwdp);

  // Attach to an iOS device by UUID.
  // @param device_id optional 40-character hex UUID, or NULL for any device
  // @param to_device_id optional selected device UUID
  // @param to_device_name optional selected device name
  // @result fd, or -1 for error
  int (*attach)(iwdp_t iwdp, const char *device_id, char **to_device_id,
                char **to_device_name, int *to_device_os_version,
                void **to_ssl_session);

  // Select the port-scan range for the browser listener.
  // @param to_port preferred port, e.g. 9227.  If a device is re-attached
  //   then this will be set to the previously-selected port
  // @param to_min_port e.g. set to 9222
  // @param to_max_port e.g. set to 9322
  iwdp_status (*select_port)(iwdp_t self, const char *device_id, int *to_port,