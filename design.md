Google BSD license <https://developers.google.com/google-bsd-license>   
Copyright 2012 Google Inc.  <wrightt@google.com>


iOS WebKit Debug Proxy Design
=============================

See the [README.md](README.md) for an overview.

Source
------

- [src/ios_webkit_debug_proxy_main.c](src/ios_webkit_debug_proxy_main.c)   
   \- The "main"   

- [src/ios_webkit_debug_proxy.c](src/ios_webkit_debug_proxy.c)    
   \- WebInspector to WebKit Remote Debugging Protocol translator   
   \- See [examples/wdp_client.js](examples/wdp_client.js) and <http://localhost:9221>   

- [src/webinspector.c](src/webinspector.c)   
   \- iOS WebInspector library   
   \- See [examples/wi_client.c](examples/wi_client.c)
   \- See [src/rpc.c](src/rpc.c) parser

- [src/device_listener.c](src/device_listener.c)   
   \- iOS device add/remove listener   
   \- See [examples/dl_client.c](examples/dl_client.c)   

- [src/websocket.c](src/websocket.c)   
   \- A generic WebSocket library   
   \- Uses base64.c and sha1.c from [PolarSSL](http://www.polarssl.org)   
   \- See [examples/ws_echo1.c](examples/ws_echo1.c) and [examples/ws_echo2.c](examples/ws_echo2.c)

- Utilities:   
   \- [src/char_buffer.c](src/char_buffer.c) byte buffer   
   \- [src/hash_table.c](src/hash_table.c) dictionary   
   \- [src/port_config.c](src/port_config.c) parses device_id:port config files   
   \- [src/socket_manager.c](src/socket_manager.c) select-based socket controller   


Architecture
------------

The high-level design is shown below:

![Alt overview](overview.png "Overview")

The various clients are shown below:

![Alt clients](clients.png "Clients")


The major components of the ios_webkit_debug_proxy are:

  1. A device_listener that listens for iOS device add/remove events
  1. A (port, webinspector) pair for each device, e.g.:   
     - [(port 9222 <--> iphoneX's inspector),
     -  (port 9223 <--> iphoneY's inspector), ...]
  1. Zero or more active WebSocket clients, e.g.:
     - [websocketA is connected to :9222/devtools/page/7, ...]
  1. A socket_manager that handles all the socket I/O


The code is object-oriented via the use of structs and function pointers.
For example, the device_listener struct defines two "public API" functions:

    dl_status (*start)(dl_t self);
    dl_status (*on_recv)(dl_t self, const char *buf, );

and three "abstract" callback functions:

    dl_status (*send)(dl_t self, const char *buf, size_t length);
    dl_status (*on_attach)(dl_t self, const char *device_id);
    dl_status (*on_detach)(dl_t self, const char *device_id);

plus a field for client use:

    void *state;

For example, [examples/dl_client.c](examples/dl_client.c) creates a listener and sets the missing callbacks:

    int fd = dl_connect();
    dl_t dl = dl_new(); // sets the "start" and "on_recv" functions
    