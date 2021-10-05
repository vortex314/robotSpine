# DRAFT : WORK IN PROGRESS
# Zenoh-proxy
Target is to enable small micro-controllers to participate in a [zenoh](http://zenoh.io) network for publishing and subscribing to data
# Approach
A first approach could be to include in the zenoh router the possibility to accept traffic directly from serial ( or I2C, SPI,.. ) lines. 
However this would create a tight coupling to the specific technology and worst of all I would have to learn yet another language : Rust

The approach taken is that a proxy is running between the serial port and the TCP connection to the router.

The code can start from zenoh-c and zenoh-pico. The zenoh-pico looks like the most straight forward one as it already supports the raw TCP communication link.

So the idea is to mimic the tcp interface at the micro-controller side and encapsulate over the serial line to the proxy running on the same machine as the router. 

The scouting interface is not supported in the uC side but can be executed by the proxy or the locator can be statically configured. 

# Target features 
- No TCP/IP stack or ethernet or wifi required.
- Enable zenoh communication to/from zenoh router
- Keep alignment with new releases of zeno-pico, create a direct code-reuse
- the proxy should support the flashing of the micro-controller ( uC ) directly through the serial line ( esptool implementation in the proxy for the ESP32 [esp-serial-flasher] (https://github.com/espressif/esp-serial-flasher). This is code that Espressif made available. The same should be true for a number of other uC.
- any data coming from the uC that is not a data frame,should be send to stdout of the proxy. This enables raw logging in the device while debugging and testing. So a single USB link is sufficient
- minimum memory footprint at uC side ( validate with STM32F103 and LM4F120 )
- Flashing command can come through zenoh itself or detect presence of file. 
# Architecture
<img src="docs/architecture.png">

# Protocol
Message Types : Client2Proxy , Proxy2Client
- TCP open (C2P) and close (C2P)
- TCP send (C2P) and recv (P2C)
- TCP connected & disconnected (P2C)

# Testing & development approach 
- Run a serial loopback via 2 USB FTDI cross-connected and develop client and proxy on same platform for easy debugging
- abstract the serial interface to make it portable 
- try to keep single thread in client to enable Arduino coverage
- Port to ESP32 the client part
- Validate Arduino approach on maple mini 
- Use PlatformIo as easy porting tool
- SPI or I2C on Raspberry Pi ? 

# Build instructions
- download zenoh-pico at the same level as this repository
```
mkdir build
cd build
cmake ..
- connect FTDI back to back 
./proxy /dev/ttyUSB0 115200
./loopback /dev/ttyUSB1 115200

```
