set(DA_ENGINE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Source")
set(DA_ENGINE_CONTENT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Content")
set(DA_ENGINE_GENERATED_DIR "${DA_GENERATED_DIR}/Core")

file(MAKE_DIRECTORY "${DA_ENGINE_GENERATED_DIR}")

configure_file(
    "${DA_ENGINE_SOURCE_DIR}/Core/EngineConfig.h.in"
    "${DA_ENGINE_GENERATED_DIR}/EngineConfig.h"
    @ONLY
)

# engine
collect_sources(DA_ENGINE_SOURCES DIRS Source)

add_library(DirectAttack SHARED ${DA_ENGINE_SOURCES})
add_library(DA::Engine ALIAS DirectAttack)

target_precompile_headers(DirectAttack PRIVATE
    "${DA_ENGINE_SOURCE_DIR}/PCH.h"
)

target_include_directories(DirectAttack
    PUBLIC
        "${DA_ENGINE_SOURCE_DIR}"
    PRIVATE
        "${DA_GENERATED_DIR}"
)

# compile-time definitions
target_compile_definitions(DirectAttack
    PRIVATE
        DA_BUILDING_LIBRARY
        DA_LOG_CATEGORY=Engine
        DA_ENGINE_CONTENT_DIR="${DA_ENGINE_CONTENT_DIR}"
        GLFW_INCLUDE_VULKAN
    PUBLIC
        DA_SHARED
)

# dependencies
target_link_libraries(DirectAttack
    PUBLIC
        Vulkan::Vulkan
        glfw
        spdlog::spdlog
        hangar::vma
    PRIVATE
        hangar::stb_image
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

# shaders: GLSL -> SPIR-V at build time
file(GLOB DA_SHADER_SOURCES CONFIGURE_DEPENDS
    "${DA_ENGINE_CONTENT_DIR}/Shaders/*.vert"
    "${DA_ENGINE_CONTENT_DIR}/Shaders/*.frag"
    "${DA_ENGINE_CONTENT_DIR}/Shaders/*.comp"
)

set(DA_SHADER_BINARIES)
foreach(SHADER IN LISTS DA_SHADER_SOURCES)
    get_filename_component(SHADER_NAME "${SHADER}" NAME)
    set(SPV "${CMAKE_BINARY_DIR}/generated/Shaders/${SHADER_NAME}.spv")

    get_filename_component(SPV_DIR "${SPV}" DIRECTORY)
    file(MAKE_DIRECTORY "${SPV_DIR}")

    add_custom_command(
        OUTPUT "${SPV}"
        COMMAND Vulkan::glslc
            --target-env=vulkan1.3
            "${SHADER}"
            -o "${SPV}"
        DEPENDS "${SHADER}"
        COMMENT "Compiling shader ${SHADER_NAME}"
        VERBATIM
    )
    list(APPEND DA_SHADER_BINARIES "${SPV}")
endforeach()

add_custom_target(DirectAttackShaders DEPENDS ${DA_SHADER_BINARIES})
add_dependencies(DirectAttack DirectAttackShaders)

create_ide_folders(DA_ENGINE_SOURCES)
register_clang_format(DA_ENGINE_SOURCES)