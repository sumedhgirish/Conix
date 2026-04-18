cmake_minimum_required(VERSION 3.10)
project(
    Conix
    VERSION 0.1
    DESCRIPTION "A Containerization framework built on the Linux Kernel"
    LANGUAGES C
)

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
