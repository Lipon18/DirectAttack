collect_sources(DA_RUNTIME_SOURCES DIRS Source)

add_library(DARuntime STATIC ${DA_RUNTIME_SOURCES})
add_library(DA::Runtime ALIAS DARuntime)

target_include_directories(DARuntime
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Include"
)

target_link_libraries(DARuntime
    PUBLIC
        DA::Engine
)

target_compile_options(DARuntime
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
        $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)

target_compile_definitions(DARuntime
    PRIVATE
        DA_LOG_CATEGORY=Runtime
)

create_ide_folders(DA_RUNTIME_SOURCES)
register_clang_format(DA_RUNTIME_SOURCES)