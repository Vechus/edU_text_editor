cmake_minimum_required(VERSION 3.16)
project(edu_api C)

set(CMAKE_C_STANDARD 11)

add_executable(edu_api nuovo_main.c)
target_link_libraries(edu_api m)