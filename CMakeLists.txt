# Specify the minimum version for CMake
cmake_minimum_required(VERSION 3.0)

# Project's name
project(Snob)

# Define the C++ compiler flags
# Note: CMake automatically handles the '-c' and '-o' flags
set(CMAKE_C_COMPILER gcc)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -fPIC -fopenmp -O3")

# Define the source files
set(SOURCE_FILES main.c dotypes.c glob.c reals.c samples.c inputs.c poplns.c classes.c expmults.c doall.c tune.c block.c listen.c tactics.c badmoves.c expbin.c vonm.c)

# Define the object files (not necessary with CMake, but listed for completeness)
# set(OBJ_FILES main.o dotypes.o glob.o reals.o samples.o inputs.o poplns.o classes.o expmults.o doall.o tune.o block.o listen.o tactics.o badmoves.o expbin.o vonm.o)

# Define the small object files (again, not necessary, but listed for completeness)
# set(SMALLOBJ_FILES snob.o dotypes.o glob.o reals.o samples.o inputs.o poplns.o classes.o expmults.o doall.o tune.o block.o listen.o tactics.o badmoves.o expbin.o vonm.o)

# Define the library object files
set(LIBOBJ_FILES dotypes.o glob.o reals.o samples.o inputs.o poplns.o classes.o expmults.o doall.o tune.o block.o listen.o tactics.o badmoves.o expbin.o vonm.o)

# Add the executable targets
add_executable(snob-factor ${SOURCE_FILES})
add_executable(snob-factors ${SOURCE_FILES})
add_executable(pro prompt.c)

# Link libraries (if any)
target_link_libraries(snob-factor m)
target_link_libraries(snob-factors m)

# Add the shared library target
add_library(libsnob SHARED ${LIBOBJ_FILES})

# Custom target for cleaning up the build directory
add_custom_target(clean
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/CleanAll.cmake
)

# Add a test target
add_custom_target(test
    COMMAND ./test.sh phi
    DEPENDS snob-factor pro snob-factors libsnob
)

# This file, CleanAll.cmake, should contain the commands to clean up the build directory

