cmake_minimum_required(VERSION 3.12)
project(spine)
add_executable(redis-brain)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 11)

add_definitions( -DLINUX -DCATCH_CONFIG_DISABLE)
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -g )
set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} -g )

set(WS /home/lieven/workspace )
set(HIREDIS ${WS}/robotSpine/hiredis )
set(BROKER ${WS}/components/broker/src)
set(LINUX ${WS}/components/linux/src)
set(TINYCBOR ${WS}/robotSpine/tinycbor )
set(JSON ${WS}/json/single_include )
set(COMMON ../common/src )
set(COMPONENTS ${WS}/components)
set(ARDUINOJSON ${WS}/ArduinoJson )


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
  -L${HIREDIS} -l:libhiredis.a
  -lrt  
  -latomic)


