cmake_minimum_required(VERSION 3.20)
project(VGCE VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


include(FetchContent)
FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG        v6.1.9
)
FetchContent_MakeAvailable(ftxui)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_executable(vgce
    src/main.cpp
    src/core/application.cpp
    src/model/search_tree.cpp
    src/tui/renderer.cpp
    src/uci/uci_client.cpp
)

if(UNIX AND NOT APPLE)
    target_sources(vgce PRIVATE src/process/platform/process_linux.cpp)
elseif(WIN32)
    target_sources(vgce PRIVATE src/process/platform/process_windows.cpp)
endif()

target_include_directories(vgce PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(vgce PRIVATE ftxui::component)

if(MSVC)
    target_compile_options(vgce PRIVATE /W4 /WX)
else()
    target_compile_options(vgce PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# Clang-format target
find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
    add_custom_target(
        format
        COMMAND ${CLANG_FORMAT} -i -style=file
                ${CMAKE_SOURCE_DIR}/src/*.cpp
                ${CMAKE_SOURCE_DIR}/src/*.hpp
                ${CMAKE_SOURCE_DIR}/src/*/*.cpp
                ${CMAKE_SOURCE_DIR}/src/*/*.hpp
        COMMENT "Formatting source code with clang-format"
    )
endif()
