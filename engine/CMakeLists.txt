# generate engine config header
set(DA_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

file(MAKE_DIRECTORY "${DA_GENERATED_DIR}/Core")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Core/EngineConfig.h.in"
    "${DA_GENERATED_DIR}/Core/EngineConfig.h"
    @ONLY
)

# engine
collect_sources(ENGINE_SOURCE_FILES DIRS Source)

add_library(DirectAttack SHARED ${ENGINE_SOURCE_FILES})

target_compile_definitions(DirectAttack PRIVATE
    DA_ENGINE_CONTENT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Content"
)

# shaders: GLSL -> SPIR-V at build time
file(GLOB DA_SHADER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Content/Shaders/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/Content/Shaders/*.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/Content/Shaders/*.comp"
)

set(DA_SHADER_BINARIES)
foreach(SHADER ${DA_SHADER_SOURCES})
    set(SPV "${SHADER}.spv")
    add_custom_command(
        OUTPUT "${SPV}"
        COMMAND Vulkan::glslc --target-env=vulkan1.3 "${SHADER}" -o "${SPV}"
        DEPENDS "${SHADER}"
        COMMENT "Compiling shader ${SHADER}"
        VERBATIM
    )
    list(APPEND DA_SHADER_BINARIES "${SPV}")
endforeach()

add_custom_target(DirectAttackShaders DEPENDS ${DA_SHADER_BINARIES})
add_dependencies(DirectAttack DirectAttackShaders)

target_precompile_headers(DirectAttack PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/PCH.h"
)

set_target_properties(DirectAttack PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Engine-bin"
)

create_ide_folders(ENGINE_SOURCE_FILES)
register_clang_format(ENGINE_SOURCE_FILES)

# source directory
target_include_directories(DirectAttack 
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    PRIVATE
        "${DA_GENERATED_DIR}"
)

# dependencies
target_link_libraries(DirectAttack PUBLIC
    Vulkan::Vulkan
    glfw
    spdlog::spdlog
    hangar::vma
    PRIVATE
        hangar::stb_image
)

target_compile_definitions(DirectAttack
    PRIVATE
        DA_BUILDING_LIBRARY
        DA_LOG_CATEGORY=Engine
        GLFW_INCLUDE_VULKAN
    PUBLIC
        DA_SHARED
)

# PUBLIC: consumers (Testbed) compile the logging macros too, so they need the
# conforming preprocessor. Each flag is its own generator expression.
target_compile_options(DirectAttack
    PUBLIC
        $<$<CXX_COMPILER_ID:MSVC>:/Zc:preprocessor>
        $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
)