collect_sources(TESTBED_SOURCE_FILES DIRS Source)

add_executable(Testbed
    ${TESTBED_SOURCE_FILES}
)

target_link_libraries(Testbed
    PRIVATE
        DirectAttack
)

target_compile_options(Testbed PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

target_compile_definitions(Testbed PRIVATE 
    DA_LOG_CATEGORY=Testbed
)

# Copy DirectAttack.dll beside Testbed.exe.
copy_runtime_dll(Testbed "$<TARGET_FILE:DirectAttack>")

create_ide_folders(TESTBED_SOURCE_FILES)
register_clang_format(TESTBED_SOURCE_FILES)