cmake_minimum_required(VERSION 3.12)
project(spine)

add_executable(serialProxy)
add_executable(udpProxy)
add_executable(cborTest)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g ")
set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -g ")
add_definitions(-DLINUX -std=c++11)
# set(CMAKE_BUILD_TYPE Debug)

file(GLOB_RECURSE HIREDIS_H 
    LIST_DIRECTORIES true 
    RELATIVE ../../../.. 
    hiredis.h )
message( STATUS ${HIREDIS_H})

find_path(HIREDIS_HEADER NAMES hiredis.h PATHS /home/lieven/workspace/robotSpine/hiredis NO_DEFAULT_PATH )
message( STATUS ${HIREDIS_HEADER})
# find_library(HIREDIS_LIB NAMES hiredis PATHS /home/lieven/workspace/robotSpine  )
find_library(HIREDIS_LIB NAMES hiredis PATHS ..  )
message( STATUS  ${HIREDIS_LIB})
# find_path(REDIS_PLUS_PLUS_HEADER sw)
# find_library(REDIS_PLUS_PLUS_LIB redis++)


set(HIREDIS /home/lieven/workspace/robotSpine/hiredis )
set(BROKER ../broker/src)
set(LINUX ../linux/src)
set(TINYCBOR /home/lieven/workspace/robotSpine/tinycbor )
set(JSON ../json )
set(COMMON ../common/src )
set(COMPONENTS ../../components)
set(ARDUINOJSON ../../ArduinoJson/src)



include_directories(
    src 
    /usr/local/include
    ${COMPONENTS}/inc
    ${COMMON}
    ${TINYCBOR}/src
    ${JSON}/single_include
    ${ARDUINOJSON}
    ${HIREDIS}
    ${BROKER}
    ${COMPONENTS}/linux
    )

target_sources(serialProxy PRIVATE 
    src/serialProxy.cpp 
    src/config.cpp 
    src/SessionSerial.cpp
    src/serial.cpp 
    ${BROKER}/BrokerRedisJson.cpp
    ${COMPONENTS}/linux/Log.cpp
    ${COMPONENTS}/linux/Sys.cpp
    ${COMPONENTS}/linux/limero.cpp
    ${COMMON}/ppp_frame.cpp 
    ${COMMON}/Frame.cpp
    ${COMMON}/frame.cpp
    ${COMMON}/Crc32.cpp
    ${COMMON}/CborDump.cpp
    ${COMPONENTS}/src/CborWriter.cpp
    ${COMPONENTS}/src/CborReader.cpp
    ${COMPONENTS}/src/StringUtility.cpp
    ) 

target_sources(udpProxy PRIVATE 
    src/udpProxy.cpp 
    src/config.cpp 
    src/SessionUdp.cpp
    src/Udp.cpp 
    ${BROKER}/BrokerRedisJson.cpp
    ${COMPONENTS}/linux/Log.cpp
    ${COMPONENTS}/linux/Sys.cpp
    ${COMPONENTS}/linux/limero.cpp
    ${COMMON}/ppp_frame.cpp 
    ${COMMON}/Frame.cpp
    ${COMMON}/frame.cpp
    ${COMMON}/Crc32.cpp
    ${COMMON}/CborDump.cpp
    ${COMPONENTS}/src/CborWriter.cpp
    ${COMPONENTS}/src/CborReader.cpp
    ${COMPONENTS}/src/StringUtility.cpp
    ) 


target_sources(cborTest PRIVATE 
    src/cborTest.cpp 
    ${COMPONENTS}/linux/Log.cpp
    ${COMPONENTS}/linux/Sys.cpp
    ${COMPONENTS}/src/StringUtility.cpp
    ) 


target_link_libraries(serialProxy 
    -L${TINYCBOR}/lib -l:libtinycbor.a
    -lpthread  
    -L${HIREDIS} -l:libhiredis.a
    -lrt  
    -latomic)

target_link_libraries(udpProxy 
    -L${TINYCBOR}/lib -l:libtinycbor.a
    -lpthread  
    -L${HIREDIS} -l:libhiredis.a
    -lrt  
    -latomic)


target_link_libraries(cborTest 
    -L${TINYCBOR}/lib -l:libtinycbor.a
    -lpthread  
    -lrt  
    -latomic)