# project info
cmake_minimum_required(VERSION 4.3.4 FATAL_ERROR)
set(PROJECT_NAME DirectAttack)
set(PROJECT_DESCRIPTION "Direct Attack Engine")
project(${PROJECT_NAME} VERSION 0.1.0 DESCRIPTION "${PROJECT_DESCRIPTION}" LANGUAGES CXX)

# c++ compiler configuration
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "_CMake")

# Output directories.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# CMake helpers.
include("${CMAKE_SOURCE_DIR}/CMAKE/CMakeHelpers.cmake")

# Hangar: fetch / install external dependencies
find_program(HANGAR_EXE NAMES hangar REQUIRED)

execute_process(
    COMMAND "${HANGAR_EXE}" install
        --manifest "${CMAKE_SOURCE_DIR}/Tools/hangar.toml"
        --profile "${CMAKE_SOURCE_DIR}/Profiles/windows-msvc-debug.toml"
        --profile "${CMAKE_SOURCE_DIR}/Profiles/windows-msvc-release.toml"
        --cmake-out "${CMAKE_BINARY_DIR}/hangar.cmake"
    RESULT_VARIABLE HANGAR_RESULT
)
if(NOT HANGAR_RESULT EQUAL 0)
    message(FATAL_ERROR "hangar install failed")
endif()

include("${CMAKE_BINARY_DIR}/hangar.cmake")

# dependencies
find_package(Vulkan REQUIRED)
find_package(glfw3 3.5 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)

# project directories
add_subdirectory(Engine)
add_subdirectory(Testbed)
enable_testing()
include(GoogleTest)
add_subdirectory(Tests/DirectAttackTests)

# Visual Studio startup project.
set_property(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    PROPERTY VS_STARTUP_PROJECT Testbed
)