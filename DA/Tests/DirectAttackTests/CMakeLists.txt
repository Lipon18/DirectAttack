project(DirectAttackTests LANGUAGES CXX)

collect_sources(DA_ENGINE_TEST_SOURCES DIRS Source)

add_executable(DirectAttackTests
    ${DA_ENGINE_TEST_SOURCES}
)

target_link_libraries(DirectAttackTests
    PRIVATE
        DA::Engine
        GTest::gtest_main
)

target_compile_options(DirectAttackTests
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
        $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)

target_compile_definitions(DirectAttackTests
    PRIVATE
        DA_LOG_CATEGORY=DirectAttackTests
)

create_ide_folders(DA_ENGINE_TEST_SOURCES)
register_clang_format(DA_ENGINE_TEST_SOURCES)

gtest_discover_tests(DirectAttackTests)