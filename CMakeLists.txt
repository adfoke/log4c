cmake_minimum_required(VERSION 3.20)

project(log4c VERSION 0.1.0 LANGUAGES C)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)
include(CTest)
find_package(Threads)

option(LOG4C_ENABLE_WARNINGS "Enable strict compiler warnings" ON)
option(LOG4C_ENABLE_SANITIZERS "Enable sanitizers in Debug builds" ON)

function(log4c_apply_project_options target_name)
    set_target_properties(${target_name} PROPERTIES
        C_STANDARD 17
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
    )

    if(LOG4C_ENABLE_WARNINGS)
        if(MSVC)
            target_compile_options(${target_name} PRIVATE /W4)
        else()
            target_compile_options(${target_name} PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wshadow
            )
        endif()
    endif()

    if(LOG4C_ENABLE_SANITIZERS AND NOT MSVC)
        target_compile_options(${target_name} PRIVATE
            $<$<CONFIG:Debug>:-fsanitize=address,undefined>
            $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
        )
        target_link_options(${target_name} PRIVATE
            $<$<CONFIG:Debug>:-fsanitize=address,undefined>
        )
    endif()
endfunction()

add_library(log4c src/log4c.c)
add_library(log4c::log4c ALIAS log4c)

target_include_directories(log4c
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

log4c_apply_project_options(log4c)

if(Threads_FOUND)
    target_link_libraries(log4c PUBLIC Threads::Threads)
    target_compile_definitions(log4c PUBLIC LOG4C_WITH_PTHREAD=1)
endif()

add_executable(log4c_example examples/basic.c)
target_link_libraries(log4c_example PRIVATE log4c)
log4c_apply_project_options(log4c_example)

add_executable(log4c_config_example examples/from_config.c)
target_link_libraries(log4c_config_example PRIVATE log4c)
log4c_apply_project_options(log4c_config_example)

if(Threads_FOUND)
    add_executable(log4c_async_example examples/async.c)
    target_link_libraries(log4c_async_example PRIVATE log4c)
    log4c_apply_project_options(log4c_async_example)
endif()

if(BUILD_TESTING)
    add_executable(log4c_tests tests/test_log4c.c)
    target_link_libraries(log4c_tests PRIVATE log4c)
    log4c_apply_project_options(log4c_tests)
    add_test(NAME log4c_tests COMMAND log4c_tests)
endif()

install(TARGETS log4c
    EXPORT log4cTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT log4cTargets
    NAMESPACE log4c::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/log4c
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/log4cConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/log4cConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/log4c
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/log4cConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/log4cConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/log4cConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/log4c
)
