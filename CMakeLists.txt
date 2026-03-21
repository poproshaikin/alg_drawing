cmake_minimum_required(VERSION 4.1)
project(soft_renderer C)

set(CMAKE_C_STANDARD 17)

find_package(X11 REQUIRED)

add_executable(soft_renderer
	src/main.c
	src/app.c
	src/display.c
	src/draw.c
	src/ui.c
)
target_link_libraries(soft_renderer PRIVATE X11::X11)
target_link_libraries(soft_renderer PRIVATE m)
