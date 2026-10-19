AUTOMAKE_OPTIONS = subdir-objects

bin_PROGRAMS = ngene

# Add the include path for libfec
AM_CPPFLAGS = $(CPPFLAGS) -I$(srcdir)/libraries/libfec

AM_LDFLAGS = $(LDFLAGS) -pthread -fPIE -pie

# Source files for the ngene executable
ngene_SOURCES = \
    frontEnd/circularBuffer.c \
    frontEnd/frontend.c \
    frontEnd/frontendThreadFunctions.c \
    frontEnd/manageUsbFrontEndConfiguration.c \
    frontEnd/megaBuffer.c \
    frontEnd/usbInterface.c \
    warmStart/manageWarmStartConfiguration.c \
    warmStart/warmStart.c \
    acquisition/acquisition.c \
    acquisition/coarseAcquisition.c \
    acquisition/confirmation.c \
    acquisition/manageAcqConfiguration.c \
    acquisition/nonCoherentSumsManager.c \
    acquisition/refinement.c \
    tracking/galileoTracking.c \
    tracking/gpsTracking.c \
    tracking/manageTrkConfiguration.c \
    tracking/tracking.c \
    tracking/trackingDiscriminatorsLoops.c \
    pvt/carrierSmoothing.c \
    pvt/dop.c \
    pvt/kalmanFilter.c \
    pvt/leastSquares.c \
    pvt/managePvtConfiguration.c \
    pvt/pvt.c \
    pvt/pvtChecks.c \
    pvt/pvtUtility.c \
    pvt/receiverTime.c \
    utility/additionalLogs.c \
    utility/checkFolder.c \
    utility/ioFunctions.c \
    utility/matrices.c \
    utility/nmeaLog.c \
    utility/rinexLog.c \
    utility/samplesHistogram.c \
    utility/argvParser.c \
    utility/closeManager.c \
    utility/mathFunctions.c \
    utility/ncursesApi.c \
    utility/profilingSupport.c \
    utility/sampleCompression.c \
    utility/samplesSpectrum.c \
    utilityGnss/cno.c \
    utilityGnss/egnosMessageParser.c \
    utilityGnss/gpsMessageParser.c \
    utilityGnss/navigationMessage.c \
    utilityGnss/reedSolomon.c \
    utilityGnss/egnosCorrections.c \
    utilityGnss/fnavMessageParser.c \
    utilityGnss/ifenLog.c \
    utilityGnss/navigationMessageParser.c \
    utilityGnss/timeConversions.c \
    utilityGnss/egnosIonoCorrections.c \
    utilityGnss/galileoMessageParser.c \
    utilityGnss/manageNavMsgConfiguration.c \
    utilityGnss/prnList.c \
    utilityGnss/viterbi.c \
    osnma/base64.c \
    osnma/cryptoFunctions.c \
    osnma/dsmData.c \
    osnma/macLookupTable.c \
    osnma/mackList.c \
    osnma/osnmaManagement.c \
    osnma/osnmaUtilities.c \
    osnma/osnmaValidations.c \
    chimera/chimeraCryptoFunctions.c \
    utilityGnss/NeQuick/ionoCalcModip.c \
    utilityGnss/NeQuick/neqCalcTopsideNe.c \
    utilityGnss/NeQuick/neqGetRayProperties.c \
    utilityGnss/NeQuick/nequick.c \
    utilityGnss/NeQuick/neqCalcBottomsideNe.c \
    utilityGnss/NeQuick/neqGetNeOnSlantRay.c \
    utilityGnss/NeQuick/neqIntegrate.c \
    utilityGnss/NeQuick/nequickComputation.c \
    utilityGnss/NeQuick/neqCalcEpstParams.c \
    utilityGnss/NeQuick/neqGetNeOnVertRay.c \
    utilityGnss/NeQuick/neqUtils.c \
    guiSupport/commModule.c \
    guiSupport/guiInterfaceFunctions.c \
    guiSupport/threadutils.c \
    receiver/channelManager.c \
    receiver/manageConfiguration.c \
    receiver/ngene.c \
    receiver/ngeneNavMesgUtility.c \
    receiver/signalHandler.c \
    receiver/manageLogs.c \
    receiver/ngeneInitialization.c \
    asmFolder/amungoNT1065.c \
    asmFolder/carrierTable.c \
    asmFolder/fft.c \
    asmFolder/jrc.c \
    asmFolder/short2char.c \
    asmFolder/sigeV3.c \
    asmFolder/stereoTriton.c \
    asmFolder/arrays.c \
    asmFolder/data_shift.c \
    asmFolder/fraunhofer.c \
    asmFolder/prnTable.c \
    asmFolder/sige.c \
    asmFolder/stereo.c \
    asmFolder/usrp.c

# Link with the libraries, including the statically built libfec
ngene_LDADD = libraries/libfec/libfec.a -lssl -lcrypto -lpci -lusb-1.0 -lm -lncurses -ltinfo

# Build the libfec.a before building ngene
libraries/libfec/libfec.a:
	$(MAKE) -C libraries/libfec

# Ensure ngene depends on libfec.a
ngene_DEPENDENCIES = libraries/libfec/libfec.a

clean-local:
	$(MAKE) -C libraries/libfec clean

