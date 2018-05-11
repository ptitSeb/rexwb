cmake_minimum_required(VERSION 2.6)

project(rexwb)

#include_directories(include)

SET(ELFLOADER_SRC
    src/main.cpp
)

add_definitions(-Wno-multichar)
#add_definitions(-DNOLIB)

add_executable(rexwb ${ELFLOADER_SRC})
target_link_libraries(rexwb m sox)
#sox
