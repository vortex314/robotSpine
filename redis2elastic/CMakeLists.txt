cmake_minimum_required(VERSION 3.12)
project(spine)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 11)

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

add_definitions(-DLINUX -std=c++11)

set(HIREDIS ../hiredis )
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

add_executable(redis2elastic)

target_compile_options(redis2elastic PRIVATE "-DBROKER_REDIS")

target_sources(redis2elastic PRIVATE
${LINUX}/Sys.cpp
${LINUX}/limero.cpp
${LINUX}/logger.cpp
${COMMON}/CborSerializer.cpp
${COMMON}/CborDeserializer.cpp
${BROKER}/BrokerRedis.cpp
${COMMON}/CborDump.cpp
src/redis2elastic.cpp
${COMMON}/util.cpp )

target_link_libraries(redis2elastic 
-L/home/lieven/workspace/tinycbor/lib -l:libtinycbor.a
-lpthread  
-lhiredis
-lrt  
-lcurl
-latomic)

