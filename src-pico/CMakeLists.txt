cmake_minimum_required(VERSION 3.12)
project(spine)

include(GNUInstallDirs)

add_definitions(-DZENOH_LINUX -DZENOH_TRANSPORT_UDP_IP -DZENOH_DEBUG=1 -DLINUX)
set (ZENOH_TRANSPORT_UDP "ON")


set(PICO /home/lieven/workspace/zenoh-pico)
set(ZENOHC /home/lieven/workspace/zenoh-c)

set(PICO_SRCS ${PICO}/src/codec.c 
  ${PICO}/src/collection.c
  ${PICO}/src/iobuf.c
  ${PICO}/src/utils.c
  ${PICO}/src/net/codec.c
  ${PICO}/src/net/internal.c
  ${PICO}/src/net/lease_task.c
  ${PICO}/src/net/msgcodec.c
  ${PICO}/src/net/property.c
  ${PICO}/src/net/read_task.c
  ${PICO}/src/net/rname.c
  ${PICO}/src/net/session.c
#  ${PICO}/src/net/unix/network.c
  ${PICO}/src/net/unix/system.c
  ${PICO}/src/compat/strdup.c
 )


include_directories(
  src 
  ${PICO}/include
../limero/inc 
../limero/linux  
../cbor11
/home/lieven/workspace/ArduinoJson/src )

add_definitions(-DCATCH_CONFIG_DISABLE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -DZENOH_LINUX -DZENOH_TRANSPORT_UDP_IP -DZENOH_DEBUG=1")
set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -g -DZENOH_LINUX -DZENOH_TRANSPORT_UDP_IP -DZENOH_DEBUG=1")

set (CMAKE_CXX_STANDARD 11)

 
# set up our program
add_executable(serial_client)
add_executable(proxy)
add_executable(proxy-rpc)
add_executable(zn_pubsub)
add_executable(myBroker)


# declare our source files
target_sources(serial_client PRIVATE src_client/serial_client.cpp  
  src/serial.cpp  
  src/ppp_frame.cpp
  ${PICO_SRCS} 
  src/config.cpp 
  src_client/unix_serial.cpp 
  src/util.cpp )

target_sources(proxy PRIVATE src/proxy.cpp 
  src/ppp_frame.cpp 
  ${PICO_SRCS} 
  src/serial.cpp 
  src/config.cpp 
  src/util.cpp
  src/tcp.cpp
  src/tcp_session.cpp
  src/serial_session.cpp) 

target_sources(proxy-rpc PRIVATE src/proxy-rpc.cpp 
  src/ppp_frame.cpp 
  src/serial.cpp 
  src/config.cpp 
  src/util.cpp
  src/serial_session.cpp
  src/zenoh_session.cpp
  ../cbor11/cbor11.cpp ) 

  target_sources(myBroker PRIVATE src/broker.cpp 
  src/util.cpp)

  target_sources(zn_pubsub PRIVATE src/zn_pubsub.c )

target_link_libraries(serial_client 
-L/home/lieven/workspace/limero/linux/build -llimero 
-lpthread  
-lrt  
-latomic)

target_link_libraries(proxy 
-L/home/lieven/workspace/limero/linux/build -llimero 
-lpthread  
-lrt  
-latomic)


target_link_libraries(proxy-rpc 
-L/home/lieven/workspace/limero/linux/build -llimero 
-L/usr/local/lib/ -lzenohc
-lpthread  
-lrt  
-latomic)


target_link_libraries(zn_pubsub 
-L/usr/local/lib/ -lzenohc
-lpthread  
-lrt  
-latomic)

target_link_libraries(myBroker 
-L/home/lieven/workspace/limero/linux/build -llimero 
-L/usr/local/lib/ -lzenohc
-lpthread  
-lrt  
-latomic)



