# CMake Helpers

# collect_sources(<OUT_VAR> DIRS <dir>... [EXTENSIONS <ext>...])
#
# Recursively collects files under each DIR (relative to the calling CMakeLists.txt).
# Default extensions: h hpp inl cpp c
function(collect_sources OUT_VAR)
    cmake_parse_arguments(ARG "" "" "DIRS;EXTENSIONS" ${ARGN})

    if(NOT ARG_DIRS)
        message(FATAL_ERROR "collect_sources: DIRS is required")
    endif()

    if(NOT ARG_EXTENSIONS)
        set(ARG_EXTENSIONS h hpp inl cpp c)
    endif()

    set(PATTERNS)
    foreach(DIR IN LISTS ARG_DIRS)
        foreach(EXT IN LISTS ARG_EXTENSIONS)
            list(APPEND PATTERNS "${CMAKE_CURRENT_SOURCE_DIR}/${DIR}/*.${EXT}")
        endforeach()
    endforeach()

    file(GLOB_RECURSE FOUND_FILES
        CONFIGURE_DEPENDS
        RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
        ${PATTERNS}
    )

    list(SORT FOUND_FILES)
    set(${OUT_VAR} ${FOUND_FILES} PARENT_SCOPE)
endfunction()

function(create_ide_folders SOURCE_FILES)

    foreach(SOURCE IN LISTS ${SOURCE_FILES})

    get_filename_component(SOURCE_ABSOLUTE "${SOURCE}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    get_filename_component(SOURCE_PATH "${SOURCE_ABSOLUTE}" DIRECTORY)
    file(RELATIVE_PATH SOURCE_PATH_REL "${CMAKE_CURRENT_SOURCE_DIR}" "${SOURCE_PATH}")
    string(REPLACE "/" "\\" GROUP_PATH "${SOURCE_PATH_REL}")

    if(SOURCE_PATH_REL STREQUAL "")
        set(GROUP_PATH "\\")
    endif()

    source_group("${GROUP_PATH}"
        FILES "${SOURCE_ABSOLUTE}"
    )
        
    endforeach()
    
endfunction()

# clang-format
find_program(CLANG_FORMAT_EXE NAMES clang-format)

# register_clang_format(<SOURCE_FILES_VAR>)
# Call from each project's CMakeLists.txt with the name of its source list variable.
function(register_clang_format SOURCE_FILES)
    foreach(SOURCE IN LISTS ${SOURCE_FILES})
        get_filename_component(SOURCE_ABSOLUTE "${SOURCE}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        set_property(GLOBAL APPEND PROPERTY CLANG_FORMAT_FILES "${SOURCE_ABSOLUTE}")
    endforeach()
endfunction()

# add_clang_format_target()
# Call once from the root CMakeLists.txt, after all add_subdirectory() calls.
function(add_clang_format_target)
    if(NOT CLANG_FORMAT_EXE)
        message(WARNING "clang-format not found, ClangFormat target will not be created")
        return()
    endif()

    #add_custom_target(ClangFormat
        #COMMAND python "${CMAKE_SOURCE_DIR}/Automation/Automation.py" clang_format
        #COMMET "Running clang-format on engine, editor and testbed sources"
        #VERBATIM
    #)

    get_property(FORMAT_FILES GLOBAL PROPERTY CLANG_FORMAT_FILES)
    list(FILTER FORMAT_FILES INCLUDE REGEX "\\.(h|hpp|inl|cpp|c)$")
    list(REMOVE_DUPLICATES FORMAT_FILES)

    # Write the list to a file so we never hit the Windows command-line length limit
    set(FILE_LIST "${CMAKE_BINARY_DIR}/clang_format_files.txt")
    string(REPLACE ";" "\n" FILE_LIST_CONTENT "${FORMAT_FILES}")
    file(WRITE "${FILE_LIST}" "${FILE_LIST_CONTENT}\n")

    add_custom_target(ClangFormat
        COMMAND "${CLANG_FORMAT_EXE}" -i --style=file "--files=${FILE_LIST}"
        COMMENT "Running clang-format on engine, editor and testbed sources"
        VERBATIM
    )
    set_target_properties(ClangFormat PROPERTIES FOLDER "_CMake")
endfunction()

# Copy a runtime DLL next to an executable after building it.
function(copy_runtime_dll TARGET DLL)
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DLL}"
            "$<TARGET_FILE_DIR:${TARGET}>"
        COMMENT "Copying runtime DLL: ${DLL}"
        VERBATIM
    )
endfunction()

# Copy the SDL runtime dependencies next to an executable.
function(copy_sdl_runtime TARGET)
    if(NOT TARGET "${TARGET}")
        message(FATAL_ERROR "copy_sdl_runtime: target '${TARGET}' does not exist")
    endif()
    if(NOT TARGET SDL3::SDL3)
        message(FATAL_ERROR "SDL3::SDL3 target was not found")
    endif()

    if(NOT TARGET SDL3_mixer::SDL3_mixer)
        message(FATAL_ERROR "SDL3_mixer::SDL3_mixer target was not found")
    endif()

    copy_runtime_dll("${TARGET}" "$<TARGET_FILE:SDL3::SDL3>")
    copy_runtime_dll("${TARGET}" "$<TARGET_FILE:SDL3_mixer::SDL3_mixer>")
endfunction()

# Put CMake generated targets into their own folder
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "_CMake")