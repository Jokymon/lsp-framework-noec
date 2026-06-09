set(CMAKE_SYSTEM_NAME Generic)

set(CAPYLANG_WASI_TARGET "wasm32-wasip1" CACHE STRING "WASI compilation target triple")

if(CMAKE_HOST_WIN32)
    set(_capylang_default_sysroot "C:/sw/wasi-sysroot-29.0")
    set(_capylang_default_sdk_bin "C:/sw/wasi-sdk-29.0-x86_64-windows/bin")
else()
    set(_capylang_default_sysroot "/opt/wasi-sysroot-29.0")
    set(_capylang_default_sdk_bin "/opt/wasi-sdk-29.0/bin")
endif()

if(DEFINED ENV{CAPYLANG_WASI_SYSROOT})
    set(CAPYLANG_WASI_SYSROOT "$ENV{CAPYLANG_WASI_SYSROOT}" CACHE PATH "Path to WASI sysroot")
else()
    set(CAPYLANG_WASI_SYSROOT "${_capylang_default_sysroot}" CACHE PATH "Path to WASI sysroot")
endif()

if(DEFINED ENV{CAPYLANG_WASI_SDK_BIN})
    set(CAPYLANG_WASI_SDK_BIN "$ENV{CAPYLANG_WASI_SDK_BIN}" CACHE PATH "Path to WASI SDK bin directory")
else()
    set(CAPYLANG_WASI_SDK_BIN "${_capylang_default_sdk_bin}" CACHE PATH "Path to WASI SDK bin directory")
endif()

find_program(CAPYLANG_CLANG
    NAMES clang clang.exe
    PATHS "${CAPYLANG_WASI_SDK_BIN}"
    NO_DEFAULT_PATH
    REQUIRED
)

set(CMAKE_SYSROOT "${CAPYLANG_WASI_SYSROOT}")

set(CMAKE_C_COMPILER ${CAPYLANG_CLANG})
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_C_COMPILER_TARGET ${CAPYLANG_WASI_TARGET})

set(CMAKE_CXX_COMPILER ${CAPYLANG_CLANG})
set(CMAKE_CXX_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_TARGET ${CAPYLANG_WASI_TARGET})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")

set(CMAKE_EXECUTABLE_SUFFIX_C .wasm)
set(CMAKE_EXECUTABLE_SUFFIX_CXX .wasm)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lc++abi -lc++")

if(NOT DEFINED CMAKE_CROSSCOMPILING_EMULATOR)
    find_program(CAPYLANG_WASMTIME
        NAMES wasmtime wasmtime.exe
    )

    if(CAPYLANG_WASMTIME)
        set(CMAKE_CROSSCOMPILING_EMULATOR "${CAPYLANG_WASMTIME};run" CACHE STRING "Command used to execute WASM binaries during build and test steps")
    endif()
endif()
