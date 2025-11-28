cmake_minimum_required(VERSION 3.10)
project(log4c C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Include directories
include_directories(include)

# Source files
set(SOURCES
    src/log4c.c
)

# Create static library
add_library(log4c STATIC ${SOURCES})

# Link pthread
find_package(Threads REQUIRED)
target_link_libraries(log4c Threads::Threads)

# Install targets
install(TARGETS log4c
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES include/log4c.h
    DESTINATION include
)

# Example program
add_executable(simple_example examples/simple_example.c)
target_link_libraries(simple_example log4c)

# Compile options
if(MSVC)
    target_compile_options(log4c PRIVATE /W4)
else()
    target_compile_options(log4c PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Conditional compilation options
option(LOG_COMPILE_DEBUG "Include DEBUG level logs at compile time" OFF)
if(LOG_COMPILE_DEBUG)
    target_compile_definitions(log4c PUBLIC LOG_COMPILE_DEBUG)
endif()

# Testing
enable_testing()
# Test cases can be added here
