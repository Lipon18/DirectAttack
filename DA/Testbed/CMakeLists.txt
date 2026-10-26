collect_sources(DA_TESTBED_SOURCES DIRS Source)

add_executable(Testbed
    ${DA_TESTBED_SOURCES}
)

target_link_libraries(Testbed
    PRIVATE
        DA::Engine
)

target_compile_options(Testbed
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
        $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)

target_compile_definitions(Testbed
    PRIVATE
        DA_LOG_CATEGORY=Testbed
)

create_ide_folders(DA_TESTBED_SOURCES)
register_clang_format(DA_TESTBED_SOURCES)