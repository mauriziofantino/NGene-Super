# Resolve the binary dependency closure after link, including opaque SDK import libraries.
function(ngene_stage_runtime target)
    if(NOT WIN32)
        return()
    endif()
    set(search ${NGENE_RUNTIME_DIRS})
    if(NGENE_WITH_HACKRF)
        list(APPEND search "$<TARGET_FILE_DIR:HackRF::HackRF>" "${HACKRF_ROOT}/bin" "$ENV{HACKRF_ROOT}/bin")
    endif()
    if(VCPKG_INSTALLED_DIR AND VCPKG_TARGET_TRIPLET)
        list(APPEND search "$<IF:$<CONFIG:Debug>,${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin,${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin>")
    endif()
    get_filename_component(compiler_dir "${CMAKE_C_COMPILER}" DIRECTORY)
    find_program(NGENE_DUMPBIN dumpbin HINTS "${compiler_dir}" REQUIRED)
    set(release_crt)
    set(debug_crt)
    foreach(dll IN LISTS CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
        # UCRT release is part of supported Windows; the SDK's debug UCRT is not.
        if(dll MATCHES "[/\\]Debug_NonRedist[/\\]" OR dll MATCHES "[/\\]ucrtbased\\.dll$")
            list(APPEND debug_crt "${dll}")
        elseif(NOT dll MATCHES "[/\\]ucrt[/\\]DLLs[/\\]")
            list(APPEND release_crt "${dll}")
        endif()
        get_filename_component(directory "${dll}" DIRECTORY)
        list(APPEND search "${directory}")
    endforeach()
    list(REMOVE_DUPLICATES search)
    string(REPLACE ";" "|" search "${search}")
    string(REPLACE ";" "|" release_crt "${release_crt}")
    string(REPLACE ";" "|" debug_crt "${debug_crt}")
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" "-Dexecutable=$<TARGET_FILE:${target}>"
            "-Dsearch=${search}" "-Dtool=${NGENE_DUMPBIN}"
            "-Dcrt=$<IF:$<CONFIG:Debug>,${debug_crt},${release_crt}>"
            -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/StageRuntime.cmake"
        VERBATIM COMMENT "Collect native runtime DLL dependencies for ${target}")
endfunction()
