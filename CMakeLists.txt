cmake_minimum_required(VERSION 3.13)
project(mirror_client C)

set(CMAKE_C_STANDARD 99)

add_executable(mirror_client main.c hash.c sender.c receiver.c sender.h)

target_link_libraries(mirror_client m)
