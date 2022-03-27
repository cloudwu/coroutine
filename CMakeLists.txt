cmake_minimum_required(VERSION 3.0)

project(coroutine)

set(LIBCOROUTINEDIR ${CMAKE_CURRENT_SOURCE_DIR} CACHE INTERNAL "coroutine library" FORCE)

include(CMakeDependentOption)
cmake_dependent_option(LIBCOROUTINE_IS_ROOT_PROJECT
        "coroutine is root project"
        ON "CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR" OFF)

add_library(coroutine coroutine.c coroutine-win.c)

if (LIBCOROUTINE_IS_ROOT_PROJECT)
add_executable(demo main.c)
target_link_libraries(demo coroutine)
endif()
