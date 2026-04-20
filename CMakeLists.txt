cmake_minimum_required(VERSION 3.10)
project(
    Conix
    VERSION 0.1
    DESCRIPTION "A Containerization framework built on the Linux Kernel"
    LANGUAGES C
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------- DEPS ------------------------------------

include(FetchContent)

FetchContent_Declare(
    argtable3
    GIT_REPOSITORY https://github.com/argtable/argtable3.git
)

# Disable argtable3's tests and examples
set(ARGTABLE3_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(ARGTABLE3_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(argtable3)

# --------------------------------- DAEMON -----------------------------------

add_executable(conixd)

target_include_directories(conixd PRIVATE include)
target_sources(conixd
    PRIVATE
    src/daemon.c
    src/engine.c
    src/containers.c
    src/commands.c
    src/process.c
    src/filesystem.c
    src/logger.c
    src/utils.c
)

target_compile_definitions(conixd PRIVATE _GNU_SOURCE)
target_link_libraries(conixd
    PRIVATE
    pthread
    uuid
)

# --------------------------------- CLIENT -----------------------------------

add_executable(conix)

target_include_directories(conix PRIVATE include)
target_sources(conix
    PRIVATE
    src/runner.c
    src/commands.c
)

target_compile_definitions(conix PRIVATE _GNU_SOURCE)
target_link_libraries(conix
    PRIVATE
    argtable3
    uuid
)
