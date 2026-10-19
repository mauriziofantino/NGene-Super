AC_INIT([ngene], [1.0], [raphael@linksfoundation.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AC_PROG_CC
AC_PROG_MAKE_SET

# Set custom CFLAGS
CFLAGS="$CFLAGS -Wall -Wextra -U_FORTIFY_SOURCE -D_FILE_OFFSET_BITS=64 -D_FORTIFY_SOURCE=0 -pthread -msse2 -mfpmath=sse -fPIE"

# Checks for pkg-config
AC_CHECK_PROG([PKG_CONFIG], [pkg-config], [yes], [no])
if test "$PKG_CONFIG" = "no"; then
  AC_MSG_ERROR([pkg-config is required but not installed])
fi

# Checks for libraries using pkg-config
PKG_CHECK_MODULES([SSL], [openssl])
PKG_CHECK_MODULES([CRYPTO], [libcrypto])
PKG_CHECK_MODULES([PCI], [libpci])
PKG_CHECK_MODULES([USB], [libusb-1.0])
PKG_CHECK_MODULES([NCURSES], [ncurses])

# Checks for header files and add include paths if found
AC_CHECK_HEADERS([openssl/ssl.h], [CPPFLAGS="$CPPFLAGS -I/usr/include/openssl"])
AC_CHECK_HEADERS([pci/pci.h], [CPPFLAGS="$CPPFLAGS -I/usr/include/pci"])
AC_CHECK_HEADERS([libusb-1.0/libusb.h], [CPPFLAGS="$CPPFLAGS -I/usr/include/libusb-1.0"])
AC_CHECK_HEADERS([ncurses.h], [CPPFLAGS="$CPPFLAGS -I/usr/include/ncurses"])

# Add custom include paths
srcdir=$(dirname "$0")
CPPFLAGS="$CPPFLAGS -I$srcdir/secure -I$srcdir/frontEnd -I$srcdir/warmStart -I$srcdir/acquisition -I$srcdir/tracking -I$srcdir/pvt -I$srcdir/utility -I$srcdir/utilityGnss -I$srcdir/osnma -I$srcdir/chimera -I$srcdir/utilityGnss/NeQuick -I$srcdir/guiSupport -I$srcdir/receiver -I$srcdir/asmFolder"

# Custom command to run the configure script in libfec
AC_CONFIG_COMMANDS([libfec_configure], [
  echo "Configuring libfec..."
  (cd libraries/libfec && ./configure) || AC_MSG_ERROR([libfec configuration failed])
])

# Ensure Makefile is generated
AC_CONFIG_FILES([Makefile])

# Output the configuration
AC_OUTPUT

