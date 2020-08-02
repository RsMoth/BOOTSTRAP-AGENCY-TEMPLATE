
// Google BSD license https://developers.google.com/google-bsd-license
// Copyright 2012 Google Inc. wrightt@google.com

//
// A minimal websocket "echo" server
//

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#endif