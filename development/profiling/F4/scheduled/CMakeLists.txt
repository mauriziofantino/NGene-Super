cmake_minimum_required(VERSION 3.26)
project(F4ScheduledReplay LANGUAGES C)

# This build tree is dedicated to the offline CLI replay.
set(NGENE_WITH_WEB OFF CACHE BOOL "" FORCE)
set(NGENE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(NGENE_PROFILE_ACQUISITION_JOBS ON CACHE BOOL "" FORCE)
get_filename_component(evo "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
find_program(NODE_EXECUTABLE node REQUIRED)
set(generator "${CMAKE_CURRENT_SOURCE_DIR}/../generate-ci8-pcps.cjs")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${generator}" "${evo}/src/dsp/search.c")
set(generated "${CMAKE_CURRENT_BINARY_DIR}/f4")
execute_process(COMMAND "${NODE_EXECUTABLE}" "${generator}" "${generated}"
    WORKING_DIRECTORY "${evo}" COMMAND_ERROR_IS_FATAL ANY)

add_subdirectory("${evo}" engine EXCLUDE_FROM_ALL)
get_target_property(dsp_sources ngene_dsp SOURCES)
if(NOT "src/dsp/search.c" IN_LIST dsp_sources)
    message(FATAL_ERROR "F4 replay could not locate EVO's search kernel")
endif()
list(REMOVE_ITEM dsp_sources "src/dsp/search.c")
list(APPEND dsp_sources "${generated}/dsp/search.c")
set_property(TARGET ngene_dsp PROPERTY SOURCES "${dsp_sources}")
add_subdirectory(".." kernels EXCLUDE_FROM_ALL)
target_link_libraries(ngene_dsp PRIVATE ngene_f4_fft_kernels)
target_compile_definitions(ngene_dsp PRIVATE CI8_PCPS_SCHEDULED=1 CI8_LAB_LINK_KERNELS)
get_target_property(f4_has_avx2 ngene_f4_fft_kernels F4_HAS_AVX2)
if(f4_has_avx2)
    target_compile_definitions(ngene_dsp PRIVATE CI8_LAB_AVX2 CI8_LAB_COMPACT_AVX2)
endif()
add_executable(ngene_f4_scheduled_replay "${evo}/apps/main.c")
set_target_properties(ngene_f4_scheduled_replay PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
target_include_directories(ngene_f4_scheduled_replay PRIVATE "${evo}/src")
target_link_libraries(ngene_f4_scheduled_replay PRIVATE ngene_core)


if(WIN32 AND f4_has_avx2)
    set(profiling "${evo}/development/profiling")
    set(batch_generator "${CMAKE_CURRENT_SOURCE_DIR}/generate-batch.cjs")
    set(batch_generated "${CMAKE_CURRENT_BINARY_DIR}/batch")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${batch_generator}"
        "${evo}/src/acquisition/gps.c" "${evo}/src/acquisition/galileo.c")
    execute_process(COMMAND "${NODE_EXECUTABLE}" "${batch_generator}" "${batch_generated}"
        WORKING_DIRECTORY "${evo}" COMMAND_ERROR_IS_FATAL ANY)
    foreach(lab ngene_f4_pcps_phases ngene_f4_pcps_batch)
        add_executable(${lab}
            "${profiling}/F2b/prepare_lab.c" "${profiling}/F2b/gps_probe.c"
            "${profiling}/F2b/galileo_probe.c" "${profiling}/F3/pcps_lab.c"
            "${generated}/dsp/search.c")
        set_target_properties(${lab} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
        target_include_directories(${lab} PRIVATE
            "${evo}/src" "${profiling}/F2b" "${profiling}/F3" "${generated}")
        target_compile_definitions(${lab} PRIVATE
            NGENE_PCPS_LAB NGENE_PCPS_PROFILE NGENE_TEST_SOURCE_DIR="${evo}"
            CI8_LAB_LINK_KERNELS CI8_LAB_COMPACT_AVX2 CI8_LAB_BUFFERED _CRT_SECURE_NO_WARNINGS)
        target_link_libraries(${lab} PRIVATE ngene_core ngene_f4_fft_kernels psapi)
    endforeach()
    target_compile_definitions(ngene_f4_pcps_phases PRIVATE CI8_PCPS_PHASES)
    target_sources(ngene_f4_pcps_batch PRIVATE pcps_batch.c)
    target_include_directories(ngene_f4_pcps_batch BEFORE PRIVATE "${batch_generated}"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_definitions(ngene_f4_pcps_batch PRIVATE F4_PCPS_BATCH CI8_PCPS_SCHEDULED)

    set(group_generator "${CMAKE_CURRENT_SOURCE_DIR}/generate-scheduled-group.cjs")
    set(group_generated "${CMAKE_CURRENT_BINARY_DIR}/group")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${group_generator}"
        "${evo}/apps/main.c" "${evo}/src/acquisition/acquisition.c"
        "${evo}/src/core/channel_dispatch.c" "${evo}/src/tracking/bank.c")
    execute_process(COMMAND "${NODE_EXECUTABLE}" "${group_generator}" "${group_generated}"
        WORKING_DIRECTORY "${evo}" COMMAND_ERROR_IS_FATAL ANY)
    add_executable(ngene_f4_group_replay scheduled_group.c
        "${group_generated}/main.c" "${group_generated}/acquisition.c"
        "${group_generated}/gps.c" "${group_generated}/galileo.c"
        "${group_generated}/channel_dispatch.c" "${group_generated}/bank.c" "${generated}/dsp/search.c")
    set_target_properties(ngene_f4_group_replay PROPERTIES
        C_STANDARD 11 C_STANDARD_REQUIRED ON)
    target_include_directories(ngene_f4_group_replay PRIVATE
        "${evo}/src" "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_definitions(ngene_f4_group_replay PRIVATE NGENE_PROFILE_ACQUISITION_JOBS=1
        CI8_PCPS_SCHEDULED CI8_LAB_LINK_KERNELS CI8_LAB_COMPACT_AVX2 CI8_LAB_BUFFERED
        _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(ngene_f4_group_replay PRIVATE ngene_core ngene_f4_fft_kernels)

endif()
