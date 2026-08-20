cmake_minimum_required(VERSION 3.20)

if(WIN32)
    project(slman LANGUAGES CXX RC)
else()
    project(slman LANGUAGES CXX)
endif()

# Prevent in-source builds so the root directory stays clean
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Please build inside a dedicated build folder, e.g. 'cmake -B build' or run build.bat / build.sh.")
endif()

# Default to Universal 2 Binary (Apple Silicon ARM64 + Intel x86_64) on macOS
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures for macOS" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ensure all compiled binaries go directly into the build root directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# Ensure 100% standalone static linking on GCC/MinGW (Zero external runtime DLL dependencies)
if(MINGW OR (CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND NOT APPLE))
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
endif()

set(CORE_SOURCES
    src/config_manager.cpp
    src/csv_engine.cpp
    src/csv_splitter.cpp
    src/dialog_utils.cpp
    src/schema_transformer.cpp
)

if(WIN32)
    set(SLMAN_RESOURCES assets/resources.rc)
else()
    set(SLMAN_RESOURCES)
endif()

# CLI Executable
add_executable(slman
    src/main.cpp
    ${CORE_SOURCES}
    ${SLMAN_RESOURCES}
)
target_include_directories(slman PRIVATE src assets)

if(WIN32)
    target_link_libraries(slman PRIVATE ole32 shell32 comdlg32)

    # GUI Executable (WIN32 subsystem) with embedded icon
    add_executable(slman_GUI WIN32
        src/gui_main.cpp
        ${CORE_SOURCES}
        assets/resources.rc
    )
    target_include_directories(slman_GUI PRIVATE src assets)
    target_link_libraries(slman_GUI PRIVATE comctl32 shell32 ole32 comdlg32 dwmapi)
endif()

