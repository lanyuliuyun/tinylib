cmake_minimum_required(VERSION 3.5.1)

project(tinylib C)

option(BUILD_DEBUG "whether to build debug version library" OFF)
option(BUILD_TEST "whether to build test program" OFF)
find_package(OpenSSL)

include_directories(${PROJECT_SOURCE_DIR})
link_directories(${LIBRARY_OUTPUT_PATH})

set(C_FLAGS
  -Wall
  -Werror
  -ggdb
  -pthread
  -rdynamic
)
if (NOT BUILD_DEBUG)
    list(APPEND C_FLAGS -O2 -DNDEBUG)
endif()
string(REPLACE ";" " " CMAKE_C_FLAGS "${C_FLAGS}")

set(tinylib_SOURCES
  tinylib/util/log.c
  tinylib/util/md5.c
  tinylib/util/time_wheel.c
  tinylib/util/url.c
  tinylib/util/util.c
  tinylib/linux/net/async_task_queue.c
  tinylib/linux/net/buffer.c
  tinylib/linux/net/channel.c
  tinylib/linux/net/inetaddr.c
  tinylib/linux/net/loop.c
  tinylib/linux/net/socket.c
  tinylib/linux/net/tcp_client.c
  tinylib/linux/net/tcp_connection.c
  tinylib/linux/net/tcp_server.c
  tinylib/linux/net/timer_queue.c
  tinylib/linux/net/udp_peer.c
)

set(ssl_SOURCES
  tinylib/ssl/dtls_endpoint.c
  tinylib/ssl/tls_client.c
)

if (OpenSSL_FOUND)
  list(APPEND tinylib_SOURCES ${ssl_SOURCES})
endif()

add_library(tinylib STATIC ${tinylib_SOURCES})

if (BUILD_TEST)
  set(EXECUTABLE_OUTPUT_PATH ${LIBRARY_OUTPUT_PATH})
  message(STATUS "Will build test cases")
  add_subdirectory(test)
endif()
