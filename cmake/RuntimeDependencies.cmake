# Resolve the binary dependency closure after link, including opaque SDK import libraries.
function(ngene_stage_runtime target)
    if(NOT WIN32 OR NOT NGENE_WITH_HACKRF)
        return()
    endif()
    set(search "$<TARGET_FILE_DIR:HackRF::HackRF>" "${HACKRF_ROOT}/bin" "$ENV{HACKRF_ROOT}/bin" ${NGENE_RUNTIME_DIRS})
    if(VCPKG_INSTALLED_DIR AND VCPKG_TARGET_TRIPLET)
        list(APPEND search "$<IF:$<CONFIG:Debug>,${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin,${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin>")
    endif()
    get_filename_component(compiler_dir "${CMAKE_C_COMPILER}" DIRECTORY)
    find_program(NGENE_DUMPBIN dumpbin HINTS "${compiler_dir}")
    string(REPLACE ";" "|" search "${search}")
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" "-Dexecutable=$<TARGET_FILE:${target}>"
            "-Dsearch=${search}" "-Dtool=${NGENE_DUMPBIN}"
            -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/StageRuntime.cmake"
        VERBATIM COMMENT "Collect native runtime DLL dependencies for ${target}")
endfunction()
