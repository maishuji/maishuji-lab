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

    add_executable(maishuji-colored-primitives examples/02-colored-primitives.cpp)
    target_link_libraries(maishuji-colored-primitives PRIVATE maishuji)
    target_compile_options(maishuji-colored-primitives PRIVATE -fno-exceptions -fno-rtti)

    add_executable(maishuji-textured-quad examples/03-textured-quad.cpp)
    target_link_libraries(maishuji-textured-quad PRIVATE maishuji)
    target_compile_options(maishuji-textured-quad PRIVATE -fno-exceptions -fno-rtti)

    add_executable(maishuji-pixel-sprites examples/04-pixel-sprites.cpp)
    target_link_libraries(maishuji-pixel-sprites PRIVATE maishuji)
    target_compile_options(maishuji-pixel-sprites PRIVATE -fno-exceptions -fno-rtti)

    add_executable(maishuji-basic-3d examples/05-basic-3d.cpp)
    target_link_libraries(maishuji-basic-3d PRIVATE maishuji)
    target_compile_options(maishuji-basic-3d PRIVATE -fno-exceptions -fno-rtti)

    add_executable(maishuji-fogged-3d examples/06-fogged-3d.cpp)
    target_link_libraries(maishuji-fogged-3d PRIVATE maishuji)
    target_compile_options(maishuji-fogged-3d PRIVATE -fno-exceptions -fno-rtti)

    add_executable(maishuji-pvr-lighting examples/07-pvr-lighting.cpp)
    target_link_libraries(maishuji-pvr-lighting PRIVATE maishuji)
    target_compile_options(maishuji-pvr-lighting PRIVATE -fno-exceptions -fno-rtti)

    add_executable(maishuji-pvr-text examples/08-pvr-text.cpp)
    target_link_libraries(maishuji-pvr-text PRIVATE maishuji)
    target_compile_options(maishuji-pvr-text PRIVATE -fno-exceptions -fno-rtti)

    add_executable(maishuji-particle-batch examples/09-particle-batch.cpp)
    target_link_libraries(maishuji-particle-batch PRIVATE maishuji)
    target_compile_options(maishuji-particle-batch PRIVATE -fno-exceptions -fno-rtti)

    add_executable(maishuji-pvr-budget examples/10-pvr-budget.cpp)
    target_link_libraries(maishuji-pvr-budget PRIVATE maishuji)
    target_compile_options(maishuji-pvr-budget PRIVATE -fno-exceptions -fno-rtti)

    add_executable(maishuji-advanced-multilingual-text
        advanced_examples/01-multilingual-text.cpp)
    target_link_libraries(maishuji-advanced-multilingual-text PRIVATE maishuji)
    target_compile_options(maishuji-advanced-multilingual-text PRIVATE
        -fno-exceptions -fno-rtti)

    add_executable(maishuji-pvr-asset
        advanced_examples/02-pvr-asset.cpp)
    target_link_libraries(maishuji-pvr-asset PRIVATE maishuji)
    target_compile_options(maishuji-pvr-asset PRIVATE
        -fno-exceptions -fno-rtti)

    add_executable(maishuji-textured-3d
        advanced_examples/03-textured-3d.cpp)
    target_link_libraries(maishuji-textured-3d PRIVATE maishuji)
    target_compile_options(maishuji-textured-3d PRIVATE
        -fno-exceptions -fno-rtti)

    add_executable(maishuji-heightmapped-terrain
        advanced_examples/04-heightmapped-terrain.cpp)
    target_link_libraries(maishuji-heightmapped-terrain PRIVATE maishuji)
    target_compile_options(maishuji-heightmapped-terrain PRIVATE
        -fno-exceptions -fno-rtti)

    add_executable(maishuji-terrain-walk
        advanced_examples/05-terrain-walk.cpp)
    target_link_libraries(maishuji-terrain-walk PRIVATE maishuji)
    target_compile_options(maishuji-terrain-walk PRIVATE
        -fno-exceptions -fno-rtti)

    function(maishuji_enable_link_map target)
        target_link_options(${target} PRIVATE
            "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${target}.map")
    endfunction()

    maishuji_enable_link_map(maishuji-pvr-smoke)
    maishuji_enable_link_map(maishuji-hello-pvr)
    maishuji_enable_link_map(maishuji-colored-primitives)
    maishuji_enable_link_map(maishuji-textured-quad)
    maishuji_enable_link_map(maishuji-pixel-sprites)
    maishuji_enable_link_map(maishuji-basic-3d)
    maishuji_enable_link_map(maishuji-fogged-3d)
    maishuji_enable_link_map(maishuji-pvr-lighting)
    maishuji_enable_link_map(maishuji-pvr-text)
    maishuji_enable_link_map(maishuji-particle-batch)
    maishuji_enable_link_map(maishuji-pvr-budget)
    maishuji_enable_link_map(maishuji-advanced-multilingual-text)
    maishuji_enable_link_map(maishuji-pvr-asset)
    maishuji_enable_link_map(maishuji-textured-3d)
    maishuji_enable_link_map(maishuji-heightmapped-terrain)
    maishuji_enable_link_map(maishuji-terrain-walk)
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
