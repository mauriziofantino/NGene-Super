set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR armv7)
set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
# Keep the hard-float ABI from the cross compiler; only the FFT kernel enables NEON.
set(CMAKE_C_FLAGS_INIT "-march=armv7-a -mfpu=vfpv3-d16")
find_program(NGENE_QEMU qemu-arm)
if(NGENE_QEMU)
    set(CMAKE_CROSSCOMPILING_EMULATOR "${NGENE_QEMU};-L;/usr/arm-linux-gnueabihf")
endif()
