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
   \- See [examp