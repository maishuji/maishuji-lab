cmake_minimum_required(VERSION 3.13)

project(maishuji-lab LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_SYSTEM_NAME STREQUAL "dreamcast")
    add_library(maishuji STATIC
        src/pvr.cpp
        src/kos/pvr_backend.cpp
    )
    target_include_directories(maishuji
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_compile_options(maishuji PRIVATE -fno-exceptions -fno-rtti)

    add_executable(maishuji-pvr-smoke examples/pvr_smoke.cpp)
    target_compile_options(maishuji-pvr-smoke PRIVATE -fno-exceptions -fno-rtti)

    add_executable(maishuji-hello-pvr examples/01-hello-pvr.cpp)
    target_link_libraries(maishuji-hello-pvr PRIVATE maishuji)
    target_compile_options(maishuji-hello-pvr PRIVATE -fno-exceptions -fno-rtti)
else()
    add_library(maishuji STATIC
        src/pvr.cpp
        src/host/recording_backend.cpp
    )
    target_include_directories(maishuji
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )

    add_executable(maishuji-host-smoke examples/host_smoke.cpp)

    add_executable(maishuji-host-tests tests/host/lifecycle_tests.cpp)
    target_include_directories(maishuji-host-tests
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/host"
    )
    target_link_libraries(maishuji-host-tests PRIVATE maishuji)
endif()
