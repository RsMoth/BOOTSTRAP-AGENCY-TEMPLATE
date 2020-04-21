# Google BSD license https://developers.google.com/google-bsd-license
# Copyright 2012 Google Inc. wrightt@google.com
#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([ios_webkit_debug_proxy], [1.8.8], [https://github.com/google/ios-webkit-debug-proxy/issues])
AM_INIT_AUTOMAKE([1.10 no-define])
AC_CONFIG_SRCDIR([src/])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL
PKG_PROG_PKG_CONFIG

# Checks for libraries.
PKG_CHECK_MODULES(libimobiledevice, libimobiledevice-1.0 >= 1.3.0)
PKG_CHECK_MODULES(libplist, libplist-2.0 >= 2.2.0)
PKG_CHECK_MODULES(openssl, openssl >= 0.9.8)
AC_CHECK_LIB([plist-2.0], [plist_to_xml],
             [ ], [AC_MSG_FAILURE([**