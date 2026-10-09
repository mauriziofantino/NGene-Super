cmake_minimum_required(VERSION 3.26)
# This runs in a separate cmake -P process, not in the parent project's policy scope.
if(POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW)
endif()
if(NOT EXISTS "${executable}")
    message(FATAL_ERROR "Runtime staging needs a linked executable")
endif()
string(REPLACE "|" ";" search "${search}")
string(REPLACE "|" ";" crt "${crt}")
list(FILTER search EXCLUDE REGEX "^/bin$")
get_filename_component(destination "${executable}" DIRECTORY)
# Executables/tests share a configuration directory. Serialize staging so parallel
# links cannot inspect a DLL while another post-build process is still copying it.
file(LOCK "${destination}/.ngene-runtime.lock" GUARD PROCESS TIMEOUT 300)
# Put the selected toolset CRT next to the binary before dependency discovery. Otherwise
# Windows may resolve an older CRT in System32 and the portable package would omit it.
foreach(dll IN LISTS crt)
    if(NOT EXISTS "${dll}")
        message(FATAL_ERROR "Selected MSVC runtime DLL is missing: ${dll}")
    endif()
    file(COPY "${dll}" DESTINATION "${destination}")
endforeach()
if(tool AND EXISTS "${tool}")
    set(CMAKE_GET_RUNTIME_DEPENDENCIES_TOOL dumpbin)
    set(CMAKE_GET_RUNTIME_DEPENDENCIES_COMMAND "${tool}")
endif()
set(CMAKE_GET_RUNTIME_DEPENDENCIES_PLATFORM windows+pe)
file(GET_RUNTIME_DEPENDENCIES EXECUTABLES "${executable}" DIRECTORIES ${search}
    RESOLVED_DEPENDENCIES_VAR resolved UNRESOLVED_DEPENDENCIES_VAR unresolved
    PRE_EXCLUDE_REGEXES "[Aa][Pp][Ii]-[Mm][Ss]-.*" "[Ee][Xx][Tt]-[Mm][Ss]-.*"
    POST_EXCLUDE_REGEXES ".*[/\\][Ss][Yy][Ss][Tt][Ee][Mm]32[/\\].*" ".*[/\\][Ss][Yy][Ss][Ww][Oo][Ww]64[/\\].*")
if(unresolved)
    message(FATAL_ERROR "Unresolved runtime DLLs: ${unresolved}. Supply NGENE_RUNTIME_DIRS (directories containing SDK dependency DLLs), then rebuild. No manual copying is required.")
endif()
foreach(dll IN LISTS resolved)
    get_filename_component(directory "${dll}" DIRECTORY)
    if(NOT directory STREQUAL destination)
        file(COPY "${dll}" DESTINATION "${destination}")
    endif()
endforeach()
