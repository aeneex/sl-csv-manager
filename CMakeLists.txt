cmake_minimum_required(VERSION 3.20)
project(slman LANGUAGES CXX RC)

# Prevent in-source builds so the root directory stays clean
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Please build inside a dedicated build folder, e.g. 'cmake -B build' or run build.bat.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ensure 100% standalone static linking (Zero external runtime DLL dependencies)
if(MINGW OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
endif()

set(CORE_SOURCES
    src/config_manager.cpp
    src/csv_engine.cpp
    src/csv_splitter.cpp
    src/dialog_utils.cpp
    src/schema_transformer.cpp
)

# CLI Executable with embedded icon
add_executable(slman
    src/main.cpp
    ${CORE_SOURCES}
    assets/resources.rc
)
target_include_directories(slman PRIVATE src assets)
target_link_libraries(slman PRIVATE ole32 shell32 comdlg32)

# GUI Executable (WIN32 subsystem) with embedded icon
add_executable(slman_GUI WIN32
    src/gui_main.cpp
    ${CORE_SOURCES}
    assets/resources.rc
)
target_include_directories(slman_GUI PRIVATE src assets)
target_link_libraries(slman_GUI PRIVATE comctl32 shell32 ole32 comdlg32 dwmapi)
