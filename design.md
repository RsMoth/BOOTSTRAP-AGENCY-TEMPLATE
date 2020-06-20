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
   \- [src/socket_manager.c](src/socket_manager.c