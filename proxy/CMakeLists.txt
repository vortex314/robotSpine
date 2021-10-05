cmake_minimum_required(VERSION 3.12)
project(spine)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 11)
find_package(iceoryx_posh CONFIG REQUIRED)
find_package(iceoryx_hoofs CONFIG REQUIRED)

get_target_property(ICEORYX_CXX_STANDARD iceoryx_posh::iceoryx_posh CXX_STANDARD)
include(IceoryxPlatform)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# <------------ add hiredis dependency --------------->
find_path(HIREDIS_HEADER hiredis)
# target_include_directories(target PUBLIC ${HIREDIS_HEADER})

find_library(HIREDIS_LIB hiredis)
# target_link_libraries(target ${HIREDIS_LIB})

# <------------ add redis-plus-plus dependency -------------->
# NOTE: this should be *sw* NOT *redis++*
find_path(REDIS_PLUS_PLUS_HEADER sw)
# target_include_directories(target PUBLIC ${REDIS_PLUS_PLUS_HEADER})

find_library(REDIS_PLUS_PLUS_LIB redis++)
# target_link_libraries(target ${REDIS_PLUS_PLUS_LIB})

add_definitions(-DZENOH_LINUX -DZENOH_TRANSPORT_UDP_IP -DZENOH_DEBUG=1 -DLINUX -std=c++11)
# set (ZENOH_TRANSPORT_UDP "ON")

# set(ZENOHC /home/lieven/workspace/zenoh-c)
set(HIREDIS ../hiredis )
# set(LIMERO /home/lieven/workspace/limero )
set(BROKER ../broker/src)
set(LINUX ../linux/src)
set(TINYCBOR ../tinycbor )
set(ETL ../etl )
set(COMMON ../common/src )


include_directories(
  src 
  /usr/local/include
  ${COMMON}
  ${TINYCBOR}/src
  ${ETL}/include
  ${HIREDIS}
  ${BROKER}
  ${LINUX}
/home/lieven/workspace/ArduinoJson/src )

add_definitions(-DCATCH_CONFIG_DISABLE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -DZENOH_LINUX -DZENOH_TRANSPORT_UDP_IP -DZENOH_DEBUG=1")
set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -g -DZENOH_LINUX -DZENOH_TRANSPORT_UDP_IP -DZENOH_DEBUG=1")
# set(CMAKE_BUILD_TYPE Debug)

set (CMAKE_CXX_STANDARD 11)

add_executable(serialProxy)

add_executable(redis-elastic)

add_executable(test)
add_executable(redis-test)
add_executable(udp-client)
add_executable(proxy-cli)

target_compile_options(serialProxy PRIVATE "-DBROKER_REDIS")
target_compile_options(redis-elastic PRIVATE "-DBROKER_REDIS")

target_sources(test PRIVATE 
${LINUX}/limero.cpp
  src/test.cpp 
  ${LINUX}/logger.cpp
  ${LINUX}/Sys.cpp
  ${COMMON}/CborSerializer.cpp
  ${COMMON}/CborDeserializer.cpp
  ${COMMON}/CborDump.cpp
 ) 

 target_sources(redis-test PRIVATE 
 ${LINUX}/limero.cpp
 src/redis-test.cpp 
 ${LINUX}/logger.cpp
 ${LINUX}/Sys.cpp
  ${COMMON}/CborSerializer.cpp
  ${COMMON}/CborDeserializer.cpp
  ${COMMON}/CborDump.cpp
 ) 

  target_sources(serialProxy PRIVATE 
  src/serialProxy.cpp 
  src/config.cpp 
  src/SessionSerial.cpp
  src/serial.cpp 
  src/SessionUdp.cpp
  src/Udp.cpp 
  ${BROKER}/BrokerRedis.cpp
  ${LINUX}/logger.cpp
  ${LINUX}/Sys.cpp
  ${LINUX}/limero.cpp
  ${COMMON}/ppp_frame.cpp 
  ${COMMON}/CborSerializer.cpp
  ${COMMON}/CborDeserializer.cpp
  ${COMMON}/CborDump.cpp
  ${COMMON}/Frame.cpp
  ${COMMON}/util.cpp ) 

  target_sources(proxy-cli PRIVATE 
  src/proxyCli.cpp 
  src/config.cpp 
  src/SessionSerial.cpp
  src/serial.cpp 
  src/SessionUdp.cpp
  src/Udp.cpp 
  ${BROKER}/BrokerRedis.cpp
  ${LINUX}/logger.cpp
  ${LINUX}/Sys.cpp
  ${LINUX}/limero.cpp
  ${COMMON}/ppp_frame.cpp 
  ${COMMON}/CborSerializer.cpp
  ${COMMON}/CborDeserializer.cpp
  ${COMMON}/CborDump.cpp
  ${COMMON}/Frame.cpp
  ${COMMON}/util.cpp ) 


target_sources(redis-elastic PRIVATE
${LINUX}/Sys.cpp
${LINUX}/limero.cpp
${LINUX}/logger.cpp
${COMMON}/CborSerializer.cpp
${COMMON}/CborDeserializer.cpp
${BROKER}/BrokerRedis.cpp
${COMMON}/CborDump.cpp
src/redis-elastic.cpp
${COMMON}/util.cpp )

target_sources(udp-client PRIVATE
${LINUX}/Sys.cpp
${LINUX}/limero.cpp
${LINUX}/logger.cpp
${COMMON}/CborSerializer.cpp
${COMMON}/CborDeserializer.cpp
${COMMON}/CborDump.cpp
src/udp_client.cpp
src/Udp.cpp
${COMMON}/util.cpp )

target_link_libraries(serialProxy 
-L/home/lieven/workspace/tinycbor/lib -l:libtinycbor.a
-lpthread  
-lhiredis
-lrt  
-latomic)

target_link_libraries(proxy-cli 
-L/home/lieven/workspace/tinycbor/lib -l:libtinycbor.a
-lpthread  
-lhiredis
-lrt  
-latomic)

target_link_libraries(redis-test 
-L/home/lieven/workspace/tinycbor/lib -l:libtinycbor.a
gtest_main
-lhiredis
-lpthread  
-lrt  
-latomic)



target_link_libraries(test 
-L/home/lieven/workspace/tinycbor/lib -l:libtinycbor.a
gtest_main
-lpthread  
-lrt  
-latomic)

target_link_libraries(redis-test 
-L/home/lieven/workspace/tinycbor/lib -l:libtinycbor.a
gtest_main
-lpthread  
-lrt  
-latomic)

target_link_libraries(redis-elastic 
-L/home/lieven/workspace/tinycbor/lib -l:libtinycbor.a
-lpthread  
-lhiredis
-lrt  
-lcurl
-latomic)

target_link_libraries(udp-client 
-L/home/lieven/workspace/tinycbor/lib -l:libtinycbor.a
gtest_main
-lpthread  
-lrt  
-latomic)



include(GoogleTest)
gtest_discover_tests(test)
