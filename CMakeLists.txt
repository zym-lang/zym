set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Tiny")

set(CMAKE_C_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")

set(CMAKE_C_FLAGS_TINY "-Oz -DNDEBUG -ffunction-sections -fdata-sections -fno-exceptions -fvisibility=hidden -fno-asynchronous-unwind-tables -fno-unwind-tables")
set(CMAKE_EXE_LINKER_FLAGS_TINY "-Wl,--gc-sections -Wl,-s")

set(PGO_MODE "OFF")
if(PGO_MODE STREQUAL "GEN")
    message(STATUS ">> PGO: GENERATION MODE - Running this build will create profile data.")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-generate")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-generate")
elseif(PGO_MODE STREQUAL "USE")
    message(STATUS ">> PGO: USE MODE - Optimizing binary using generated data.")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-use -fprofile-correction")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-use -fprofile-correction")
endif()

set(CMAKE_BUILD_TYPE Release)

cmake_minimum_required(VERSION 4.0)
project(src C)
include(CheckIPOSupported)
check_ipo_supported(RESULT ZYM_LTO_OK OUTPUT ZYM_LTO_ERR)

set(CMAKE_C_STANDARD 11)

# add_compile_definitions(DEBUG_CALL)

add_subdirectory(zym_core)

set(APP_SOURCES
        src/main.c
        src/runtime_loader.h
        src/runtime_loader.c
        src/full_executor.h
        src/full_executor.c
)

set(ZYM_NATIVE_SOURCES
        src/natives/natives.h
        src/natives/natives.c
        src/natives/marshal.h
        src/natives/marshal.c
        src/natives/print.c
        src/natives/console.c
        src/natives/util/time.c
        src/natives/util/random.c
        src/natives/buffer.c
        src/natives/io.c
        src/natives/os.c
        src/natives/ZymVM.c
        src/natives/process.c
)

add_executable(zym ${APP_SOURCES} ${ZYM_NATIVE_SOURCES})
if(ZYM_LTO_OK)
    set_property(TARGET zym PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    set_property(TARGET zym PROPERTY INTERPROCEDURAL_OPTIMIZATION_TINY TRUE)
endif()

target_link_libraries(zym PRIVATE zym_core)
