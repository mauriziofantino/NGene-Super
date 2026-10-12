# Native SDR providers reuse the dependency bootstrap, SYSTEM headers and DLL
# staging already used by HackRF. Upstream sources remain outside first-party /WX.
function(ngene_usb_target)
    if(TARGET ngene_libusb)
        return()
    endif()
    find_path(NGENE_USB_INCLUDE_DIR libusb.h PATH_SUFFIXES libusb-1.0 REQUIRED)
    find_library(NGENE_USB_LIBRARY NAMES usb-1.0 libusb-1.0 REQUIRED)
    add_library(ngene_libusb UNKNOWN IMPORTED GLOBAL)
    set_target_properties(ngene_libusb PROPERTIES IMPORTED_LOCATION "${NGENE_USB_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${NGENE_USB_INCLUDE_DIR}"
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${NGENE_USB_INCLUDE_DIR}")
    if(WIN32 AND VCPKG_INSTALLED_DIR AND VCPKG_TARGET_TRIPLET)
        find_library(NGENE_USB_LIBRARY_DEBUG NAMES usb-1.0 libusb-1.0
            PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/lib" NO_DEFAULT_PATH REQUIRED)
        set_target_properties(ngene_libusb PROPERTIES IMPORTED_CONFIGURATIONS "Debug;Release"
            IMPORTED_LOCATION_DEBUG "${NGENE_USB_LIBRARY_DEBUG}"
            IMPORTED_LOCATION_RELEASE "${NGENE_USB_LIBRARY}"
            MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release MAP_IMPORTED_CONFIG_MINSIZEREL Release)
    endif()
endfunction()

function(ngene_resolve_rtlsdr)
    set(NGENE_RTLSDR_PROVIDER AUTO CACHE STRING "RTL-SDR provider: AUTO, SYSTEM or SOURCE")
    set_property(CACHE NGENE_RTLSDR_PROVIDER PROPERTY STRINGS AUTO SYSTEM SOURCE)
    if(NOT NGENE_RTLSDR_PROVIDER MATCHES "^(AUTO|SYSTEM|SOURCE)$")
        message(FATAL_ERROR "Invalid RTL-SDR provider")
    endif()
    if(NOT NGENE_RTLSDR_PROVIDER STREQUAL "SOURCE")
        find_path(NGENE_RTLSDR_INCLUDE_DIR rtl-sdr.h)
        find_library(NGENE_RTLSDR_LIBRARY NAMES rtlsdr)
        if(NGENE_RTLSDR_INCLUDE_DIR AND NGENE_RTLSDR_LIBRARY)
            add_library(RTLSDR::RTLSDR UNKNOWN IMPORTED GLOBAL)
            set_target_properties(RTLSDR::RTLSDR PROPERTIES IMPORTED_LOCATION "${NGENE_RTLSDR_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${NGENE_RTLSDR_INCLUDE_DIR}"
                INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${NGENE_RTLSDR_INCLUDE_DIR}")
            if(WIN32 AND VCPKG_INSTALLED_DIR AND VCPKG_TARGET_TRIPLET)
                find_library(NGENE_RTLSDR_LIBRARY_DEBUG NAMES rtlsdr
                    PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/lib" NO_DEFAULT_PATH REQUIRED)
                set_target_properties(RTLSDR::RTLSDR PROPERTIES IMPORTED_CONFIGURATIONS "Debug;Release"
                    IMPORTED_LOCATION_DEBUG "${NGENE_RTLSDR_LIBRARY_DEBUG}"
                    IMPORTED_LOCATION_RELEASE "${NGENE_RTLSDR_LIBRARY}")
            endif()
            return()
        elseif(NGENE_RTLSDR_PROVIDER STREQUAL "SYSTEM")
            message(FATAL_ERROR "RTL-SDR SDK not found")
        endif()
    endif()
    include(FetchContent)
    FetchContent_Declare(ngene_rtlsdr_source
        URL "https://codeload.github.com/osmocom/rtl-sdr/tar.gz/refs/tags/v2.0.2"
        URL_HASH "SHA256=d69943eb32df742bc38a00ce6615e41250fd57851174e5ff916ec31e9e9e68e9"
        SYSTEM DOWNLOAD_EXTRACT_TIMESTAMP FALSE SOURCE_SUBDIR ngene-library-only)
    FetchContent_MakeAvailable(ngene_rtlsdr_source)
    ngene_usb_target()
    set(source "${ngene_rtlsdr_source_SOURCE_DIR}")
    set(files librtlsdr.c tuner_e4k.c tuner_fc0012.c tuner_fc0013.c tuner_fc2580.c tuner_r82xx.c)
    list(TRANSFORM files PREPEND "${source}/src/")
    add_library(ngene_rtlsdr_native SHARED ${files})
    ngene_external_target(ngene_rtlsdr_native)
    target_include_directories(ngene_rtlsdr_native SYSTEM PUBLIC "${source}/include")
    target_compile_definitions(ngene_rtlsdr_native PRIVATE rtlsdr_EXPORTS)
    target_link_libraries(ngene_rtlsdr_native PRIVATE ngene_libusb Threads::Threads)
    if(WIN32)
        find_package(PThreads4W CONFIG REQUIRED)
        target_link_libraries(ngene_rtlsdr_native PRIVATE PThreads4W::PThreads4W)
        target_compile_definitions(ngene_rtlsdr_native PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_definitions(ngene_rtlsdr_native PRIVATE _DEFAULT_SOURCE)
        target_link_libraries(ngene_rtlsdr_native PRIVATE m)
    endif()
    set_target_properties(ngene_rtlsdr_native PROPERTIES OUTPUT_NAME rtlsdr)
    add_library(RTLSDR::RTLSDR ALIAS ngene_rtlsdr_native)
    install(TARGETS ngene_rtlsdr_native RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
    install(FILES "${source}/COPYING" DESTINATION share/ngene/licenses RENAME librtlsdr-COPYING)
endfunction()

function(ngene_resolve_bladerf)
    set(NGENE_BLADERF_PROVIDER AUTO CACHE STRING "bladeRF provider: AUTO, SYSTEM or SOURCE")
    set_property(CACHE NGENE_BLADERF_PROVIDER PROPERTY STRINGS AUTO SYSTEM SOURCE)
    if(NOT NGENE_BLADERF_PROVIDER MATCHES "^(AUTO|SYSTEM|SOURCE)$")
        message(FATAL_ERROR "Invalid bladeRF provider")
    endif()
    if(NOT NGENE_BLADERF_PROVIDER STREQUAL "SOURCE")
        find_path(NGENE_BLADERF_INCLUDE_DIR libbladeRF.h)
        find_library(NGENE_BLADERF_LIBRARY NAMES bladeRF libbladeRF)
        if(NGENE_BLADERF_INCLUDE_DIR AND NGENE_BLADERF_LIBRARY)
            add_library(BladeRF::BladeRF UNKNOWN IMPORTED GLOBAL)
            set_target_properties(BladeRF::BladeRF PROPERTIES IMPORTED_LOCATION "${NGENE_BLADERF_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${NGENE_BLADERF_INCLUDE_DIR}"
                INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${NGENE_BLADERF_INCLUDE_DIR}")
            ngene_bladerf_import_header("${NGENE_BLADERF_INCLUDE_DIR}" BladeRF::BladeRF)
            return()
        elseif(NGENE_BLADERF_PROVIDER STREQUAL "SYSTEM")
            message(FATAL_ERROR "bladeRF SDK not found")
        endif()
    endif()
    include(FetchContent)
    FetchContent_Declare(ngene_bladerf_source
        URL "https://codeload.github.com/Nuand/bladeRF/tar.gz/refs/tags/libbladeRF_v2.6.0"
        URL_HASH "SHA256=e56ec6f31863fc5b1e71896f4f0f2e530073db8a806550a284cb99beb3733624"
        SYSTEM DOWNLOAD_EXTRACT_TIMESTAMP FALSE SOURCE_SUBDIR ngene-library-only)
    FetchContent_Declare(ngene_ad936x_source
        URL "https://codeload.github.com/analogdevicesinc/no-OS/tar.gz/0bba46e6f6f75785a65d425ece37d0a04daf6157"
        URL_HASH "SHA256=b77a86780e82ff592dd3cbcbf916d6f421f5cb7b523b800255483b1992aea88d"
        SYSTEM DOWNLOAD_EXTRACT_TIMESTAMP FALSE SOURCE_SUBDIR ngene-library-only)
    FetchContent_MakeAvailable(ngene_bladerf_source ngene_ad936x_source)
    ngene_usb_target()
    include("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/BladeRFSource.cmake")
    ngene_build_bladerf("${ngene_bladerf_source_SOURCE_DIR}" "${ngene_ad936x_source_SOURCE_DIR}")
    add_library(BladeRF::BladeRF ALIAS ngene_bladerf_native)
endfunction()

# Upstream 2.6.0 exports its public API unconditionally on Windows. Keep the
# dependency's licensed header untouched and provide a separate consumer view.
function(ngene_bladerf_import_header source target)
    if(NOT WIN32)
        return()
    endif()
    set(directory "${CMAKE_CURRENT_BINARY_DIR}/generated/bladerf-import")
    file(MAKE_DIRECTORY "${directory}")
    file(READ "${source}/libbladeRF.h" header)
    string(REPLACE "__declspec(dllexport)" "__declspec(dllimport)" header "${header}")
    string(REPLACE "__attribute__ ((dllexport))" "__attribute__ ((dllimport))" header "${header}")
    file(WRITE "${directory}/libbladeRF.h" "${header}")
    configure_file("${source}/bladeRF1.h" "${directory}/bladeRF1.h" COPYONLY)
    configure_file("${source}/bladeRF2.h" "${directory}/bladeRF2.h" COPYONLY)
    target_include_directories(${target} SYSTEM BEFORE INTERFACE "${directory}")
endfunction()
