cmake_minimum_required(VERSION 4.1)
project(soft_renderer C)

set(CMAKE_C_STANDARD 17)

find_package(X11 REQUIRED)

add_executable(soft_renderer main.c)
target_link_libraries(soft_renderer PRIVATE X11::X11)
