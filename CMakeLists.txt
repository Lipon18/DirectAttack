# project info
cmake_minimum_required(VERSION  4.3.4 FATAL_ERROR)
cmake_policy(VERSION  4.3.4)
set(PROJECT_NAME DirectAttack)
project(${PROJECT_NAME} VERSION 0.1.0 LANGUAGE CXX)

# c++ compiler configuration
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Hangar: fetch / install external dependencies
find_program(HANGAR_EXE NAMES hangar REQUIRED)

execute_process(
    COMMAND "${HANGAR_EXE}" install
        --manifest "${CMAKE_SOURCE_DIR}/hangar.toml"
        --profile "${CMAKE_SOURCE_DIR}/profiles/windows-msvc.toml"
        --cmake-out "${CMAKE_BINARY_DIR}/hangar.cmake"
    RESULT_VARIABLE HANGAR_RESULT
)
if(NOT HANGAR_RESULT EQUAL 0)
    message(FATAL_ERROR "hangar install failed")
endif()
include("${CMAKE_BINARY_DIR}/hangar.cmake")

# Vulkan SDK
find_package(Vulkan REQUIRED)

# glfw
find_package(glfw3 3.5 CONFIG REQURIED)

add_executable(${PROJECT_NAME} src/main.cpp)

# CMake helpers
include("${CMAKE_SOURCE_DIR}/CMAKE/CMakeHelpers.cmake")

target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    glfw
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    GLFW_INCLUDE_VULKAN
)

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)