cmake_minimum_required(VERSION 3.20)
project(slman LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
