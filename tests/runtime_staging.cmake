cmake_minimum_required(VERSION 3.26)
# Host-side contract using synthetic dumpbin output, not native Windows execution.
set(fixture "${work}/runtime fixture")
file(REMOVE_RECURSE "${fixture}")
file(MAKE_DIRECTORY "${fixture}/sdk" "${fixture}/System32" "${fixture}/SysWOW64"
    "${fixture}/CRT" "${fixture}/run")
foreach(dll hackrf.dll libusb-1.0.dll pthreadvc3.dll)
    file(WRITE "${fixture}/sdk/${dll}" "SDK fixture ${dll}\n")
endforeach()
file(WRITE "${fixture}/System32/kernel32.dll" "OS fixture\n")
file(WRITE "${fixture}/SysWOW64/bcrypt.dll" "OS fixture\n")
file(WRITE "${fixture}/CRT/vcruntime140.dll" "Release CRT fixture\n")
file(WRITE "${fixture}/run/ngene.exe" "Executable fixture\n")
file(WRITE "${fixture}/dumpbin" [=[#!/bin/sh
case "$2" in
 *.exe) printf '    kernel32.dll\r\n    bcrypt.dll\r\n    hackrf.dll\r\n    vcruntime140.dll\r\n    api-ms-win-crt-runtime-l1-1-0.dll\r\n' ;;
 *hackrf.dll) printf '    libusb-1.0.dll\r\n    pthreadvc3.dll\r\n' ;;
 *) printf '\r\n' ;;
esac
]=])
file(CHMOD "${fixture}/dumpbin" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
set(arguments "-Dexecutable=${fixture}/run/ngene.exe"
    "-Dsearch=${fixture}/sdk|${fixture}/System32|${fixture}/SysWOW64"
    "-Dtool=${fixture}/dumpbin" "-Dcrt=${fixture}/CRT/vcruntime140.dll"
    -P "${source}/cmake/StageRuntime.cmake")
execute_process(COMMAND "${CMAKE_COMMAND}" ${arguments}
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT result EQUAL 0 OR "${output}${errors}" MATCHES "[Ww]arning")
    message(FATAL_ERROR "Runtime staging failed or warned: ${output}\n${errors}")
endif()
foreach(dll hackrf.dll libusb-1.0.dll pthreadvc3.dll vcruntime140.dll)
    file(READ "${fixture}/run/${dll}" staged)
    if(dll STREQUAL "vcruntime140.dll")
        set(origin "${fixture}/CRT/${dll}")
    else()
        set(origin "${fixture}/sdk/${dll}")
    endif()
    file(READ "${origin}" original)
    if(NOT staged STREQUAL original)
        message(FATAL_ERROR "Staged runtime differs from the selected source: ${dll}")
    endif()
endforeach()
if(EXISTS "${fixture}/run/kernel32.dll" OR EXISTS "${fixture}/run/bcrypt.dll")
    message(FATAL_ERROR "Windows OS DLLs must not be shipped with NGene")
endif()
# Missing transitive runtime DLLs must fail the build, rather than create a broken package.
file(REMOVE "${fixture}/sdk/pthreadvc3.dll" "${fixture}/run/pthreadvc3.dll")
execute_process(COMMAND "${CMAKE_COMMAND}" ${arguments}
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(result EQUAL 0 OR NOT errors MATCHES "Unresolved runtime DLLs: pthreadvc3.dll")
    message(FATAL_ERROR "An unresolved transitive DLL was not diagnosed: ${output}\n${errors}")
endif()
message(STATUS "PASS: standalone runtime policy, spaces, transitive DLL closure, CRT staging, OS exclusions and unresolved-DLL failure (mock dumpbin)")
