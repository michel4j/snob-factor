# Specify the minimum version for CMake
cmake_minimum_required(VERSION 3.5..3.27)

# Project's name
project(Snob)

# Define the C++ compiler flags
# Note: CMake automatically handles the '-c' and '-o' flags

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_C_COMPILER gcc)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -fPIC -fopenmp -O3")
endif ()
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_C_COMPILER gcc)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -fPIC -fopenmp -O3")
endif ()


# Define the source files
set(SOURCE_FILES dotypes.c glob.c reals.c samples.c inputs.c poplns.c classes.c expmults.c doall.c tune.c block.c listen.c tactics.c badmoves.c expbin.c vonm.c)

# Add the executable targets
add_executable(snob-factor ${SOURCE_FILES} main.c)
add_executable(snob-factors ${SOURCE_FILES} snob.c)

# Link libraries (if any)
target_link_libraries(snob-factor m)
target_link_libraries(snob-factors m)

# Add the shared library target
add_library(snob SHARED ${SOURCE_FILES})
