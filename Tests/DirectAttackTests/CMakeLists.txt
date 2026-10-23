project(DirectAttackTests LANGUAGES CXX)

collect_sources(DIRECT_ATTACK_TEST_SOURCE_FILES DIRS Source)

add_executable(DirectAttackTests
    ${DIRECT_ATTACK_TEST_SOURCE_FILES}
)

target_link_libraries(DirectAttackTests
    PRIVATE
        DirectAttack
        GTest::gtest_main
)

target_compile_options(DirectAttackTests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

target_compile_definitions(DirectAttackTests PRIVATE 
    DA_LOG_CATEGORY=DirectAttackTests
)

# Copy DirectAttack.dll beside DirectAttackTests.exe
copy_runtime_dll(DirectAttackTests "$<TARGET_FILE:DirectAttack>")

create_ide_folders(DIRECT_ATTACK_TEST_SOURCE_FILES)
register_clang_format(DIRECT_ATTACK_TEST_SOURCE_FILES)

gtest_discover_tests(DirectAttackTests)