#===============================================================#
# CMake config                                                  #
#===============================================================#

cmake_minimum_required(VERSION 3.0.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#===============================================================#
# Project config                                                #
#===============================================================#

project(hltools VERSION 0.5.0 LANGUAGES C)

set(PROJECT_FLAGS
    -Wno-discarded-qualifiers
)

if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND PROJECT_FLAGS
        -funroll-loops
        -fexpensive-optimizations
    )
endif()

set(PROJECT_LIBRARIES m)

#===============================================================#
# MDL Decompiler                                                #
#===============================================================#

add_executable(decompmdl
    src/common.c
    src/math.c
    src/decompile.c
    src/model.c
    src/texture.c
    src/animation.c
)

target_compile_options(decompmdl PRIVATE ${PROJECT_FLAGS})

target_link_libraries(decompmdl PRIVATE ${PROJECT_LIBRARIES})
