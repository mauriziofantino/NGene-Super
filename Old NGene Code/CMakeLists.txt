cmake_minimum_required(VERSION 3.10)
project(ngene VERSION 1.0)

#Check of the architecture
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set(ARCH "x86_64")
    message(STATUS "Target architecture: x86_64 (PC)")

elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set(ARCH "ARM64")
    message(STATUS "Target architecture: ARM64 (Raspberry Pi 4 64-bit)")

elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "armv7l")
    set(ARCH "ARM32")
    message(STATUS "Target architecture: ARM32 (Odroid XU4 or Raspberry Pi 4 32-bit)")

elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
    set(ARCH "ARM")
    message(STATUS "Target architecture: Generic ARM")

else()
    set(ARCH "UNKNOWN")
    message(WARNING "Unknown architecture detected: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

# Normal Flags
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -U_FORTIFY_SOURCE -D_FILE_OFFSET_BITS=64 -D_FORTIFY_SOURCE=0 -pthread -msse2 -mfpmath=sse -fPIE")

# Optimized
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -march=native -mtune=native -Wall -Wextra -U_FORTIFY_SOURCE -D_FILE_OFFSET_BITS=64 -D_FORTIFY_SOURCE=0 -pthread -msse2 -mfpmath=sse -fexpensive-optimizations -funroll-loops -fPIE -flto -fwhole-program -fomit-frame-pointer -fstrict-aliasing -ffast-math -fno-math-errno -fno-trapping-math -fassociative-math -fno-signed-zeros -ftree-vectorize -funswitch-loops -fno-stack-protector -fprofile-use -fprofile-correction -fopt-info-vec -fopt-info-omp -DNDEBUG -s")

# Optimized with graphite
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -march=native -mtune=native -Wall -Wextra -U_FORTIFY_SOURCE -D_FILE_OFFSET_BITS=64 -D_FORTIFY_SOURCE=0 -pthread -msse2 -mfpmath=sse -fexpensive-optimizations -funroll-loops -fPIE -flto -fwhole-program -fomit-frame-pointer -fstrict-aliasing -ffast-math -fno-math-errno -fno-trapping-math -fassociative-math -fno-signed-zeros -ftree-vectorize -floop-nest-optimize -funswitch-loops -fno-stack-protector -fprofile-use -fprofile-correction -fopt-info-vec -fopt-info-omp -DNDEBUG -s")


include_directories(
    ${PROJECT_SOURCE_DIR}/secure
    ${PROJECT_SOURCE_DIR}/frontEnd
    ${PROJECT_SOURCE_DIR}/warmStart
    ${PROJECT_SOURCE_DIR}/acquisition
    ${PROJECT_SOURCE_DIR}/tracking
    ${PROJECT_SOURCE_DIR}/pvt
    ${PROJECT_SOURCE_DIR}/utility
    ${PROJECT_SOURCE_DIR}/utilityGnss
    ${PROJECT_SOURCE_DIR}/osnma
    ${PROJECT_SOURCE_DIR}/chimera
    ${PROJECT_SOURCE_DIR}/utilityGnss/NeQuick
    ${PROJECT_SOURCE_DIR}/guiSupport
    ${PROJECT_SOURCE_DIR}/receiver
    ${PROJECT_SOURCE_DIR}/asmFolder
    ${PROJECT_SOURCE_DIR}/libraries/libfec  # Correct inclusion of libfec
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SSL REQUIRED openssl)
pkg_check_modules(CRYPTO REQUIRED libcrypto)
pkg_check_modules(USB REQUIRED libusb-1.0)
pkg_check_modules(NCURSES REQUIRED ncurses)

# Add all necessary source files from Makefile.am
add_executable(ngene
    receiver/ngene.c
    receiver/manageConfiguration.c
    receiver/signalHandler.c
    receiver/manageLogs.c
    receiver/ngeneInitialization.c
    receiver/channelManager.c
    frontEnd/circularBuffer.c
    frontEnd/frontend.c
    frontEnd/frontendThreadFunctions.c
    frontEnd/megaBuffer.c
    frontEnd/usbInterface.c
    frontEnd/manageUsbFrontEndConfiguration.c
    acquisition/acquisition.c
    acquisition/coarseAcquisition.c
    acquisition/confirmation.c
    acquisition/manageAcqConfiguration.c
    acquisition/nonCoherentSumsManager.c
    acquisition/refinement.c
    tracking/tracking.c
    tracking/galileoTracking.c
    tracking/gpsTracking.c
    tracking/manageTrkConfiguration.c
    tracking/trackingDiscriminatorsLoops.c
    pvt/pvt.c
    pvt/pvtChecks.c
    pvt/carrierSmoothing.c
    pvt/dop.c
    pvt/kalmanFilter.c
    pvt/leastSquares.c
    pvt/managePvtConfiguration.c
    pvt/pvtUtility.c
    pvt/receiverTime.c
    utility/additionalLogs.c
    utility/checkFolder.c
    utility/ioFunctions.c
    utility/matrices.c
    utility/nmeaLog.c
    utility/rinexLog.c
    utility/samplesHistogram.c
    utility/argvParser.c
    utility/closeManager.c
    utility/mathFunctions.c
    utility/ncursesApi.c
    utility/profilingSupport.c
    utility/sampleCompression.c
    utility/samplesSpectrum.c
    osnma/base64.c
    osnma/cryptoFunctions.c
    osnma/dsmData.c
    osnma/macLookupTable.c
    osnma/mackList.c
    osnma/osnmaManagement.c
    osnma/osnmaUtilities.c
    osnma/osnmaValidations.c
    chimera/chimeraCryptoFunctions.c
    utilityGnss/cno.c
    utilityGnss/egnosCorrections.c
    utilityGnss/egnosIonoCorrections.c
    utilityGnss/egnosMessageParser.c
    utilityGnss/fnavMessageParser.c
    utilityGnss/galileoMessageParser.c
    utilityGnss/gpsMessageParser.c
    utilityGnss/ifenLog.c
    utilityGnss/manageNavMsgConfiguration.c
    utilityGnss/navigationMessage.c
    utilityGnss/navigationMessageParser.c
    utilityGnss/prnList.c
    utilityGnss/reedSolomon.c
    utilityGnss/timeConversions.c
    utilityGnss/viterbi.c
    utilityGnss/NeQuick/ionoCalcModip.c
    utilityGnss/NeQuick/neqCalcTopsideNe.c
    utilityGnss/NeQuick/neqGetRayProperties.c
    utilityGnss/NeQuick/nequick.c
    utilityGnss/NeQuick/neqCalcBottomsideNe.c
    utilityGnss/NeQuick/neqGetNeOnSlantRay.c
    utilityGnss/NeQuick/neqIntegrate.c
    utilityGnss/NeQuick/nequickComputation.c
    utilityGnss/NeQuick/neqCalcEpstParams.c
    utilityGnss/NeQuick/neqGetNeOnVertRay.c
    utilityGnss/NeQuick/neqUtils.c
    guiSupport/commModule.c
    guiSupport/guiInterfaceFunctions.c
    guiSupport/threadutils.c
    asmFolder/fft.c
    asmFolder/amungoNT1065.c
    asmFolder/arrays.c
    asmFolder/data_shift.c
    asmFolder/carrierTable.c
    asmFolder/jrc.c
    asmFolder/prnTable.c
    asmFolder/short2char.c
    asmFolder/sige.c
    asmFolder/sigeV3.c
    asmFolder/stereo.c
    asmFolder/stereoTriton.c
    asmFolder/usrp.c
    asmFolder/fraunhofer.c
    warmStart/manageWarmStartConfiguration.c
    warmStart/warmStart.c
)

# Link the necessary libraries
target_link_libraries(ngene
    ${SSL_LIBRARIES}
    ${CRYPTO_LIBRARIES}
    ${USB_LIBRARIES}
    ${NCURSES_LIBRARIES}
    m
    ncurses
    tinfo
    ${PROJECT_SOURCE_DIR}/libraries/libfec/libfec.a
)

# Correct the logic to prevent re-running of ./configure
set(LIBFEC_CONFIGURE_STAMP ${PROJECT_SOURCE_DIR}/libraries/libfec/.configured_stamp)

add_custom_command(
    OUTPUT ${LIBFEC_CONFIGURE_STAMP}
    COMMAND ./configure && touch ${LIBFEC_CONFIGURE_STAMP}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/libraries/libfec
    COMMENT "Running libfec configure script (only once)"
)

add_custom_target(libfec_build
    COMMAND ${CMAKE_MAKE_PROGRAM} -C ${PROJECT_SOURCE_DIR}/libraries/libfec
    DEPENDS ${LIBFEC_CONFIGURE_STAMP}
    COMMENT "Building libfec library"
)

# Make sure ngene depends on the libfec build
add_dependencies(ngene libfec_build)

# Set installation path to home directory
set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.ngene" CACHE PATH "Installation directory" FORCE)

# Create the necessary directories in the home folder
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory \$ENV{HOME}/.ngene/bin)")

# Copy sample configuration files from the repository tree to the .ngene directory in the home folder
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/sampleConfigurationFiles \$ENV{HOME}/.ngene)")

# Install the ngene executable into the bin directory
install(TARGETS ngene RUNTIME DESTINATION bin)

# Add the bin directory to the user's PATH persistently and source the updated .bashrc
install(CODE "
    file(WRITE \$ENV{HOME}/.ngene/add_to_path.sh \"
    # Add .ngene/bin to the user's PATH
    if [[ \\\$PATH != *\\\"$HOME/.ngene/bin\\\"* ]]; then
        export PATH=\\\$PATH:\\\$HOME/.ngene/bin
    fi
    \")
    
    # Add the source command to .bashrc if it doesn't exist
    execute_process(COMMAND /bin/bash -c \"if ! grep -q 'source \$HOME/.ngene/add_to_path.sh' \$HOME/.bashrc; then echo 'source \$HOME/.ngene/add_to_path.sh' >> \$HOME/.bashrc; fi\")

    # Source the updated .bashrc to make the changes effective in the current session
    execute_process(COMMAND /bin/bash -c \"source \$HOME/.bashrc\")
")

# Add a custom uninstall target
add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -E remove_directory "$ENV{HOME}/.ngene"
    COMMAND sed -i '/source \$HOME\\/\\.ngene\\/add_to_path\\.sh/d' "$ENV{HOME}/.bashrc"
    COMMENT "Uninstalling ngene and removing .ngene folder and PATH entry"
)
