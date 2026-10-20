# engine
collect_sources(ENGINE_SOURCE_FILES DIRS src)

add_library(DirectAttack STATIC ${ENGINE_SOURCE_FILES})

create_ide_folders(ENGINE_SOURCE_FILES)

# source directory
target_include_directories(DirectAttack PUBLIC 
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# dependencies
target_link_libraries(DirectAttack PUBLIC
    Vulkan::Vulkan    
    glfw3::glfw3
)

target_compile_definitions(DirectAttack PRIVATE
    GLFW_INCLUDE_VULKAN
)

target_compile_options(DirectAttack PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)