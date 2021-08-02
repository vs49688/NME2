cmake_minimum_required(VERSION 3.10)
project(NME2)

add_executable(nme
        bitmanip.c
        bitmanip.h

        defs.h

        NME2.c
        pcb.c
        utils.c
        utils.h
        wwrif.c
        wwriff.h
)

set_target_properties(nme PROPERTIES
        CMAKE_C_STANDARD 11
        CMAKE_C_STANDARD_REQUIRED ON
)

target_compile_definitions(nme PUBLIC -DUNICODE -D_UNICODE)

