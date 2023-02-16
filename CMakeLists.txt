cmake_minimum_required (VERSION 3.13)
project (CuToolbox-Monitor)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS OFF)

file(GLOB_RECURSE SRC
    "${CMAKE_CURRENT_LIST_DIR}/src/*.c"
)
set(INCS
    "${CMAKE_CURRENT_LIST_DIR}/src"
)

add_executable(ct_monitor ${SRC})
target_include_directories(ct_monitor PRIVATE ${INCS})
