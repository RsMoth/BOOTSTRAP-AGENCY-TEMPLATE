
# iOS WebKit Debug Proxy

The ios_webkit_debug_proxy (aka _iwdp_) proxies requests from usbmuxd daemon over a websocket connection, allowing developers to send commands to MobileSafari and UIWebViews on real and simulated iOS devices.

## Installation

iOS WebKit Debug Proxy works on Linux, MacOS & Windows.

### MacOS

It's easiest to install with [homebrew](http://brew.sh/):

```console
brew install ios-webkit-debug-proxy
```
### Windows
It's easiest to install with [scoop](http://scoop.sh/):
```
scoop bucket add extras
scoop install ios-webkit-debug-proxy
```
Note: you also need the latest version of [iTunes](https://www.apple.com/il/itunes/download/) installed.

### Linux

Install dependencies available in apt repository:
```console
sudo apt-get install autoconf automake libusb-dev libusb-1.0-0-dev libplist-dev libtool libssl-dev
```

Build and install dependencies that require more recent versions:
- [libimobiledevice](https://github.com/libimobiledevice/libimobiledevice)
- [libusbmuxd](https://github.com/libimobiledevice/libusbmuxd)
- [usbmuxd](https://github.com/libimobiledevice/usbmuxd)
- [libplist](https://github.com/libimobiledevice/libplist)

Build and install `ios-webkit-debug-proxy`:
```console
git clone https://github.com/google/ios-webkit-debug-proxy.git
cd ios-webkit-debug-proxy

./autogen.sh
make
sudo make install
```

## Usage

On Linux, you must run the `usbmuxd` daemon.  The above install adds a /lib/udev rule to start the daemon whenever a device is attached.

To verify that usbmuxd can list your attached device(s), ensure that `libimobiledevice-utils` is installed and then run `idevice_id -l`.

### Start the simulator or device

The iOS Simulator is supported, but it must be started **before** the proxy.  The simulator can be started in XCode,  standalone, or via the command line:

```sh
# Xcode changes these paths frequently, so doublecheck them
SDK_DIR="/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs"
SIM_APP="/Applications/Xcode.app/Contents/Developer/Applications/Simulator.app/Contents/MacOS/Simulator"
$SIM_APP -SimulateApplication $SDK_DIR/iPhoneSimulator8.4.sdk/Applications/MobileSafari.app/MobileSafari
```

#### Enable the inspector

Your attached iOS devices must have ≥1 open browser tabs and the inspector enabled via:
  `Settings > Safari > Advanced > Web Inspector = ON`

### Start the proxy

```console
ios_webkit_debug_proxy
```

* `--debug` for verbose output.
* `--frontend` to specify a frontend
* `--help` for more options.