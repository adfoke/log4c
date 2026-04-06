cmake_minimum_required(VERSION 3.20)

project(my_app LANGUAGES C)

set(LOG4C_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH "Path to log4c")

add_subdirectory("${LOG4C_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/_deps/log4c")

add_executable(my_app
    src/main.c
)

set_target_properties(my_app PROPERTIES
    C_STANDARD 17
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

target_link_libraries(my_app PRIVATE log4c::log4c)
