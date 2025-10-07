cmake_minimum_required(VERSION 3.10)
project(log4c C)

# 设置C标准
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 包含目录
include_directories(include)

# 源文件
set(SOURCES
    src/log4c.c
)

# 创建静态库
add_library(log4c STATIC ${SOURCES})

# 链接pthread
find_package(Threads REQUIRED)
target_link_libraries(log4c Threads::Threads)

# 安装目标
install(TARGETS log4c
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES include/log4c.h
    DESTINATION include
)

# 示例程序
add_executable(simple_example examples/simple_example.c)
target_link_libraries(simple_example log4c)

# 编译选项
if(MSVC)
    target_compile_options(log4c PRIVATE /W4)
else()
    target_compile_options(log4c PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 条件编译选项
option(LOG_COMPILE_DEBUG "编译时包含DEBUG级别日志" OFF)
if(LOG_COMPILE_DEBUG)
    target_compile_definitions(log4c PUBLIC LOG_COMPILE_DEBUG)
endif()

# 测试
enable_testing()
# 这里可以添加测试用例
