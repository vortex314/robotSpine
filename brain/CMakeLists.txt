cmake_minimum_required(VERSION 3.12)
project(spine)
add_executable(redis-brain)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 11)

add_definitions( -DLINUX -DCATCH_CONFIG_DISABLE)
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -g )
set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} -g )

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

set(HIREDIS ../hiredis )
set(BROKER ../broker/src)
set(TINYCBOR ../tinycbor/src )
set(JSON ../json/single_include )
set(ETL ../etl )
set(COMMON ../common/src )
set(LINUX ../linux/src )

include_directories(
  src 
  /usr/local/include
  ${COMMON}
  ${LINUX}
  ${TINYCBOR}
  ${ETL}/include
  ${HIREDIS}
  ${BROKER}
  ${JSON} )


target_sources(redis-brain PRIVATE
${LINUX}/Sys.cpp
${LINUX}/limero.cpp
${LINUX}/logger.cpp
${BROKER}/BrokerRedisJson.cpp
src/brain.cpp
${COMMON}/util.cpp )

target_link_libraries(redis-brain 
-L/home/lieven/workspace/tinycbor/lib -l:libtinycbor.a
-lpthread  
-lhiredis
-lrt  
-latomic)


