cmake_minimum_required(VERSION 3.26)
project(network_laboratory C)

set(CMAKE_C_STANDARD 17)

add_executable(tcp_client tcp/vc_client.c)
add_executable(tcp_server tcp/vc_server.c)
add_executable(udp_client udp/dg_client.c)
add_executable(udp_server udp/dg_server.c)
