
add_executable(test_async_task test_async_task.c)
target_link_libraries(test_async_task tinylib)

add_executable(test_mt_async_task test_mt_async_task.c)
target_link_libraries(test_mt_async_task tinylib)

add_executable(test_atomic test_atomic.c)
target_link_libraries(test_atomic tinylib pthread)

add_executable(test_log test_log.c)
target_link_libraries(test_log tinylib)

add_executable(test_loop_timer test_loop_timer.c)
target_link_libraries(test_loop_timer tinylib)

add_executable(test_mt_timer test_mt_timer.c)
target_link_libraries(test_mt_timer tinylib)

add_executable(test_md5 test_md5.c)
target_link_libraries(test_md5 tinylib)

add_executable(test_tcp_server test_tcp_server.c)
target_link_libraries(test_tcp_server tinylib)

add_executable(test_tcp_client test_tcp_client.c)
target_link_libraries(test_tcp_client tinylib)

add_executable(test_tcp_client_bench test_tcp_client_bench.c)
target_link_libraries(test_tcp_client_bench tinylib)

add_executable(test_time_wheel test_time_wheel.c)
target_link_libraries(test_time_wheel tinylib)

add_executable(test_url test_url.c)
target_link_libraries(test_url tinylib)

add_executable(test_pingpong test_pingpong.c)
target_link_libraries(test_pingpong tinylib)

add_executable(test_udp_peer test_udp_peer.c)
target_link_libraries(test_udp_peer tinylib)

if (SSL_LIBRARY)
  add_executable(test_dtls_endpoint test_dtls_endpoint.c)
  target_link_libraries(test_dtls_endpoint tinylib ssl)

  add_executable(test_tls_client test_tls_client.c)
  target_link_libraries(test_tls_client tinylib ssl)

  add_executable(test_tls_client_bench test_tls_client_bench.c)
  target_link_libraries(test_tls_client_bench tinylib ssl)
endif()
