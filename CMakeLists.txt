cmake_minimum_required(VERSION 3.16.3)
project(rdp)

add_executable(server server.c)
add_executable(client client.c)
add_executable(rdp rdpd.c)

target_link_libraries(client m pthread)
target_link_libraries(server m pthread)
target_link_libraries(rdp m pthread)
