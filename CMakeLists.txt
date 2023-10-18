#===============================================================#
# CMake config                                                  #
#===============================================================#

cmake_minimum_required(VERSION 3.0.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#===============================================================#
# Project config                                                #
#===============================================================#

project(hltools VERSION 0.8.2 LANGUAGES C)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
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
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
    set(PROJECT_FLAGS
        /D_CRT_SECURE_NO_WARNINGS
        /Dstrcasecmp=_stricmp
        /Dstrdup=_strdup
    )

    set(PROJECT_LIBRARIES winmm)
endif()

#===============================================================#
# MDL Decompiler                                                #
#===============================================================#

add_executable(decompmdl
    src/common.c
    src/math.c
    src/decompile.c
    src/studio.c
    src/model.c
    src/texture.c
    src/animation.c
    src/sprite.c
    src/wad.c
)

target_precompile_headers(decompmdl PRIVATE src/pch.h)

target_compile_options(decompmdl PRIVATE ${PROJECT_FLAGS})

target_link_libraries(decompmdl PRIVATE ${PROJECT_LIBRARIES})
