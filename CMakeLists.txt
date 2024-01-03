# Specify the minimum version for CMake
cmake_minimum_required(VERSION 3.5..3.27)
set (CMAKE_C_STANDARD 11)

project(snob)

# Define the C compiler flags
# Note: CMake automatically handles the '-c' and '-o' flags

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_C_COMPILER gcc)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -fPIC -O3")
endif ()
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_C_COMPILER gcc)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -fPIC -O3")
endif ()

# Define the source files
set(SOURCE_FILES 
    ${CMAKE_SOURCE_DIR}/src/dotypes.c
    ${CMAKE_SOURCE_DIR}/src/glob.c
    ${CMAKE_SOURCE_DIR}/src/reals.c
    ${CMAKE_SOURCE_DIR}/src/samples.c
    ${CMAKE_SOURCE_DIR}/src/inputs.c
    ${CMAKE_SOURCE_DIR}/src/poplns.c
    ${CMAKE_SOURCE_DIR}/src/classes.c
    ${CMAKE_SOURCE_DIR}/src/expmults.c
    ${CMAKE_SOURCE_DIR}/src/doall.c
    ${CMAKE_SOURCE_DIR}/src/tune.c
    ${CMAKE_SOURCE_DIR}/src/block.c
    ${CMAKE_SOURCE_DIR}/src/listen.c
    ${CMAKE_SOURCE_DIR}/src/tactics.c
    ${CMAKE_SOURCE_DIR}/src/badmoves.c
    ${CMAKE_SOURCE_DIR}/src/expbin.c
    ${CMAKE_SOURCE_DIR}/src/vonm.c
)

# Share Library target for Python
add_library(snob SHARED ${SOURCE_FILES})
set_target_properties (snob PROPERTIES PREFIX "")
set_target_properties (snob PROPERTIES OUTPUT_NAME "_snob")
set_target_properties (snob PROPERTIES SUFFIX ".so")
target_link_libraries(snob m)

# Interactive SNOB Factor target
add_executable(snob-factor ${SOURCE_FILES} ${CMAKE_SOURCE_DIR}/src/main.c)
target_link_libraries(snob-factor m)

# Auto SNOB target
add_executable(auto-snob ${SOURCE_FILES} ${CMAKE_SOURCE_DIR}/src/snob.c)
target_link_libraries(auto-snob m)

