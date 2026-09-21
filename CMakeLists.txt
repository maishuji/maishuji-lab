cmake_minimum_required(VERSION 3.13)

project(maishuji-lab LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_SYSTEM_NAME STREQUAL "dreamcast")
    add_executable(maishuji-pvr-smoke examples/pvr_smoke.cpp)
    target_compile_options(maishuji-pvr-smoke PRIVATE -fno-exceptions -fno-rtti)
else()
    add_executable(maishuji-host-smoke examples/host_smoke.cpp)
endif()
