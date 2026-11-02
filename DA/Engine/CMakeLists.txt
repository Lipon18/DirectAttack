set(DA_ENGINE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Source")
set(DA_ENGINE_CONTENT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Content")
set(DA_SHADER_SOURCE_DIR "${DA_ENGINE_CONTENT_DIR}/Shaders")
set(DA_ENGINE_GENERATED_DIR "${DA_GENERATED_DIR}/Core")
set(DA_SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/generated/Shaders")

file(MAKE_DIRECTORY "${DA_ENGINE_GENERATED_DIR}")
file(MAKE_DIRECTORY "${DA_SHADER_OUTPUT_DIR}")

configure_file(
    "${DA_ENGINE_SOURCE_DIR}/Core/EngineConfig.h.in"
    "${DA_ENGINE_GENERATED_DIR}/EngineConfig.h"
    @ONLY
)

# Engine
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

# Compile definitions
target_compile_definitions(DirectAttack
    PRIVATE
        DA_BUILDING_LIBRARY
        DA_LOG_CATEGORY=Engine
        DA_ENGINE_CONTENT_DIR="${DA_ENGINE_CONTENT_DIR}"
        DA_SHADER_OUTPUT_DIR="${DA_SHADER_OUTPUT_DIR}"
        GLFW_INCLUDE_VULKAN
    PUBLIC
        DA_SHARED
        GLM_FORCE_DEPTH_ZERO_TO_ONE
)

# Dependencies
target_link_libraries(DirectAttack
    PUBLIC
        Vulkan::Vulkan
        glfw
        spdlog::spdlog
        hangar::vma
        hangar::glm
    PRIVATE
        hangar::stb_image
        hangar::cgltf
)

# Compiler options
target_compile_options(DirectAttack
    PUBLIC
        $<$<CXX_COMPILER_ID:MSVC>:/Zc:preprocessor>
        $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# Shader configuration
option(DA_ENABLE_SLANG "Enable Slang shader compilation" ON)

# GLSL compiler
if(NOT TARGET Vulkan::glslc)
    message(FATAL_ERROR
        "Vulkan::glslc was not found. Check your Vulkan SDK installation."
    )
endif()

# Slang compiler
if(DA_ENABLE_SLANG)
    find_program(DA_SLANGC_EXECUTABLE
        NAMES slangc
        HINTS
            "$ENV{VULKAN_SDK}/Bin"
            "C:/Slang/bin"
    )
    if(NOT DA_SLANGC_EXECUTABLE)
        message(FATAL_ERROR
            "Slang is enabled but slangc was not found. "
            "Set DA_SLANGC_EXECUTABLE or install Slang."
        )
    endif()
    message(STATUS "Slang shader compilation enabled")
    message(STATUS "Slang compiler: ${DA_SLANGC_EXECUTABLE}")
else()
    message(STATUS "Slang shader compilation disabled")
endif()

# Shader source discovery
file(GLOB_RECURSE DA_GLSL_SHADER_SOURCES
    CONFIGURE_DEPENDS
    LIST_DIRECTORIES false
    "${DA_SHADER_SOURCE_DIR}/GLSL/*.vert"
    "${DA_SHADER_SOURCE_DIR}/GLSL/*.frag"
    "${DA_SHADER_SOURCE_DIR}/GLSL/*.comp"
)

set(DA_SLANG_SHADER_SOURCES)

if(DA_ENABLE_SLANG)
    file(GLOB_RECURSE DA_SLANG_SHADER_SOURCES
        CONFIGURE_DEPENDS
        LIST_DIRECTORIES false
        "${DA_SHADER_SOURCE_DIR}/Slang/*.vert.slang"
        "${DA_SHADER_SOURCE_DIR}/Slang/*.frag.slang"
        "${DA_SHADER_SOURCE_DIR}/Slang/*.comp.slang"
    )
endif()

message(STATUS "Found ${DA_GLSL_SHADER_SOURCES} GLSL shader sources")
message(STATUS "Found ${DA_SLANG_SHADER_SOURCES} Slang shader sources")

# Shader compilation

set(DA_SHADER_BINARIES)

# GLSL -> SPIR-V
foreach(SHADER IN LISTS DA_GLSL_SHADER_SOURCES)
    file(RELATIVE_PATH SHADER_RELATIVE
        "${DA_SHADER_SOURCE_DIR}"
        "${SHADER}"
    )
    set(SPV
        "${DA_SHADER_OUTPUT_DIR}/${SHADER_RELATIVE}.spv"
    )
    get_filename_component(SPV_DIR
        "${SPV}"
        DIRECTORY
    )
    if(SHADER_RELATIVE MATCHES "\\.vert$")
        set(SHADER_STAGE vertex)
    elseif(SHADER_RELATIVE MATCHES "\\.frag$")
        set(SHADER_STAGE fragment)
    elseif(SHADER_RELATIVE MATCHES "\\.comp$")
        set(SHADER_STAGE compute)
    else()
        message(FATAL_ERROR
            "Unknown GLSL shader stage: ${SHADER}"
        )
    endif()
    add_custom_command(
        OUTPUT "${SPV}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${SPV_DIR}"
        COMMAND Vulkan::glslc
            --target-env=vulkan1.3
            "${SHADER}"
            -o "${SPV}"
        DEPENDS
            "${SHADER}"
        COMMENT
            "GLSL -> SPIR-V (${SHADER_STAGE}): ${SHADER_RELATIVE}"
        VERBATIM
    )
    list(APPEND DA_SHADER_BINARIES "${SPV}")
endforeach()

# Slang -> SPIR-V
foreach(SHADER IN LISTS DA_SLANG_SHADER_SOURCES)
    file(RELATIVE_PATH SHADER_RELATIVE
        "${DA_SHADER_SOURCE_DIR}"
        "${SHADER}"
    )
    set(SPV
        "${DA_SHADER_OUTPUT_DIR}/${SHADER_RELATIVE}.spv"
    )
    get_filename_component(SPV_DIR
        "${SPV}"
        DIRECTORY
    )
    if(SHADER_RELATIVE MATCHES "\\.vert\\.slang$")
        set(SHADER_STAGE vertex)
    elseif(SHADER_RELATIVE MATCHES "\\.frag\\.slang$")
        set(SHADER_STAGE fragment)
    elseif(SHADER_RELATIVE MATCHES "\\.comp\\.slang$")
        set(SHADER_STAGE compute)
    else()
        message(FATAL_ERROR
            "Unknown Slang shader stage: ${SHADER}"
        )
    endif()
    add_custom_command(
        OUTPUT "${SPV}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${SPV_DIR}"
        COMMAND "${DA_SLANGC_EXECUTABLE}"
            "${SHADER}"
            -target spirv
            -profile spirv_1_5
            -entry main
            -stage "${SHADER_STAGE}"
            -o "${SPV}"
        DEPENDS
            "${SHADER}"
        COMMENT
            "Slang -> SPIR-V (${SHADER_STAGE}): ${SHADER_RELATIVE}"

        VERBATIM
    )
    list(APPEND DA_SHADER_BINARIES "${SPV}")
endforeach()

# Shader build target
if(DA_SHADER_BINARIES)
    add_custom_target(DirectAttackShaders
        DEPENDS ${DA_SHADER_BINARIES}
    )
else()
    add_custom_target(DirectAttackShaders)
    message(WARNING
        "No shader sources were found in ${DA_SHADER_SOURCE_DIR}"
    )
endif()

add_dependencies(DirectAttack DirectAttackShaders)

# IDE
create_ide_folders(DA_ENGINE_SOURCES)
register_clang_format(DA_ENGINE_SOURCES)