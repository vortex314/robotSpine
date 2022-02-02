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
set(JSON ../json/single_include )
set(ARDUINOJSON ../../ArduinoJson)
set(COMPONENTS ../../components/ )

include_directories(
  src 
  /usr/local/include
  ${COMPONENTS}/inc
  ${COMPONENTS}/linux
  ${HIREDIS}
  ${JSON} 
  ${ARDUINOJSON}/src)


target_sources(redis-brain PRIVATE
    ${COMPONENTS}/linux/Sys.cpp
    ${COMPONENTS}/linux/limero.cpp
    ${COMPONENTS}/linux/Log.cpp
    ${COMPONENTS}/linux/BrokerRedisJson.cpp
    src/brain.cpp
    ${COMPONENTS}/src/StringUtility.cpp )

target_link_libraries(redis-brain 
-L/home/lieven/workspace/tinycbor/lib -l:libtinycbor.a
-lpthread  
-lhiredis
-lrt  
-latomic)


