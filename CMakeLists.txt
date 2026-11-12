cmake_minimum_required(VERSION 3.22)

project(VSTPlugins VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(PLUGINS_COPY_AFTER_BUILD "Copy built plugins into the system plugin folders (needs admin on Windows)" OFF)

add_subdirectory(libs/JUCE)

# Shared header-only libraries
add_library(pixelui INTERFACE)
target_include_directories(pixelui INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/shared)

add_library(sharedsp INTERFACE)
target_include_directories(sharedsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/shared)

# DSP unit tests (no JUCE)
add_executable(dsp_test tests/dsp_test.cpp)
target_link_libraries(dsp_test PRIVATE sharedsp)

# Plugins
add_subdirectory(plugins/CUtTA)
add_subdirectory(plugins/Sub)
add_subdirectory(plugins/IronS)
