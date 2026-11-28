/** @file broadcast.c @brief Classic GPS SF1..3 and Galileo words0..5 orbit/clock parsing.
 * Field maps and scale factors adapted from gpsMessageParser.c and galileoMessageParser.c,
 * Mario Nicola, 2014. Disk/global receiver effects removed; cache is per channel. */
#include "ngene/navigation.h"
#include <math.h>
#include <string.h>
/** Radian conversion factor for broadcast angles expressed in semicircles. */
#define NG_BROADCAST_PI 3.14159265358979323846
/** Set Boolean state in decoded navigation data. */
#define TRUE_FLAG 1
/** Clear Boolean state in decoded navigation data. */
#define FALSE_FLAG 0
/** Successful navigation parser result. */
#define SUCCESS_RETURN 0
/** Unsuccessful navigation parser result. */
#define FAILURE_RETURN -1
/** Decoded health state that permits signal use. */
#define SIGNAL_OK 0
/** Decoded health state that excludes signal use. */
#define SIGNAL_OUT_OF_SERVICE 1
/** Upper bound on accepted GPS reference times in seconds of week. */
#define GPS_MAXIMUM_TOC 604784.0
/** Upper bound on accepted GPS orbit eccentricity. */
#define GPS_MAXIMUM_ECCENTRICITY 0.03
/** Start byte of week number in GPS LNAV subframe 1. */
#define GPS_SUBF1_WN_FIRST_BYTE_IDX (8)
/** End byte of week number in GPS LNAV subframe 1. */
#define GPS_SUBF1_WN_LAST_BYTE_IDX (9)
/** Start-byte extraction width of week number in GPS LNAV subframe 1. */
#define GPS_SUBF1_WN_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of week number in GPS LNAV subframe 1. */
#define GPS_SUBF1_WN_BITS_OF_LAST_BYTE (4)
/** Start byte of satellite health in GPS LNAV subframe 1. */
#define GPS_SUBF1_HEALTH_FIRST_BYTE_IDX (10)
/** End byte of satellite health in GPS LNAV subframe 1. */
#define GPS_SUBF1_HEALTH_LAST_BYTE_IDX (10)
/** Start-byte extraction width of satellite health in GPS LNAV subframe 1. */
#define GPS_SUBF1_HEALTH_BITS_OF_FIRST_BYTE (0)
/** End-byte extraction width of satellite health in GPS LNAV subframe 1. */
#define GPS_SUBF1_HEALTH_BITS_OF_LAST_BYTE (6)
/** Start byte of user range accuracy index in GPS LNAV subframe 1. */
#define GPS_SUBF1_URA_FIRST_BYTE_IDX (9)
/** End byte of user range accuracy index in GPS LNAV subframe 1. */
#define GPS_SUBF1_URA_LAST_BYTE_IDX (10)
/** Start-byte extraction width of user range accuracy index in GPS LNAV subframe 1. */
#define GPS_SUBF1_URA_BITS_OF_FIRST_BYTE (2)
/** End-byte extraction width of user range accuracy index in GPS LNAV subframe 1. */
#define GPS_SUBF1_URA_BITS_OF_LAST_BYTE (2)
/** Start byte of high part of clock issue of data in GPS LNAV subframe 1. */
#define GPS_SUBF1_IODC_MS_FIRST_BYTE_IDX (11)
/** End byte of high part of clock issue of data in GPS LNAV subframe 1. */
#define GPS_SUBF1_IODC_MS_LAST_BYTE_IDX (11)
/** Start-byte extraction width of high part of clock issue of data in GPS LNAV subframe 1. */
#define GPS_SUBF1_IODC_MS_BITS_OF_FIRST_BYTE (2)
/** End-byte extraction width of high part of clock issue of data in GPS LNAV subframe 1. */
#define GPS_SUBF1_IODC_MS_BITS_OF_LAST_BYTE (0)
/** Start byte of low part of clock issue of data in GPS LNAV subframe 1. */
#define GPS_SUBF1_IODC_LS_FIRST_BYTE_IDX (28)
/** End byte of low part of clock issue of data in GPS LNAV subframe 1. */
#define GPS_SUBF1_IODC_LS_LAST_BYTE_IDX (29)
/** Start-byte extraction width of low part of clock issue of data in GPS LNAV subframe 1. */
#define GPS_SUBF1_IODC_LS_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of low part of clock issue of data in GPS LNAV subframe 1. */
#define GPS_SUBF1_IODC_LS_BITS_OF_LAST_BYTE (2)
/** Start byte of clock reference time in GPS LNAV subframe 1. */
#define GPS_SUBF1_TOC_FIRST_BYTE_IDX (29)
/** End byte of clock reference time in GPS LNAV subframe 1. */
#define GPS_SUBF1_TOC_LAST_BYTE_IDX (31)
/** Start-byte extraction width of clock reference time in GPS LNAV subframe 1. */
#define GPS_SUBF1_TOC_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of clock reference time in GPS LNAV subframe 1. */
#define GPS_SUBF1_TOC_BITS_OF_LAST_BYTE (2)
/** Start byte of clock bias in GPS LNAV subframe 1. */
#define GPS_SUBF1_AF0_FIRST_BYTE_IDX (36)
/** End byte of clock bias in GPS LNAV subframe 1. */
#define GPS_SUBF1_AF0_LAST_BYTE_IDX (38)
/** Start-byte extraction width of clock bias in GPS LNAV subframe 1. */
#define GPS_SUBF1_AF0_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of clock bias in GPS LNAV subframe 1. */
#define GPS_SUBF1_AF0_BITS_OF_LAST_BYTE (8)
/** Width in bits of clock bias in GPS LNAV subframe 1. */
#define GPS_SUBF1_AF0_FIELD_DIM (22)
/** Start byte of clock drift in GPS LNAV subframe 1. */
#define GPS_SUBF1_AF1_FIRST_BYTE_IDX (33)
/** End byte of clock drift in GPS LNAV subframe 1. */
#define GPS_SUBF1_AF1_LAST_BYTE_IDX (35)
/** Start-byte extraction width of clock drift in GPS LNAV subframe 1. */
#define GPS_SUBF1_AF1_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of clock drift in GPS LNAV subframe 1. */
#define GPS_SUBF1_AF1_BITS_OF_LAST_BYTE (2)
/** Width in bits of clock drift in GPS LNAV subframe 1. */
#define GPS_SUBF1_AF1_FIELD_DIM (16)
/** Start byte of clock drift rate in GPS LNAV subframe 1. */
#define GPS_SUBF1_AF2_FIRST_BYTE_IDX (32)
/** End byte of clock drift rate in GPS LNAV subframe 1. */
#define GPS_SUBF1_AF2_LAST_BYTE_IDX (33)
/** Start-byte extraction width of clock drift rate in GPS LNAV subframe 1. */
#define GPS_SUBF1_AF2_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of clock drift rate in GPS LNAV subframe 1. */
#define GPS_SUBF1_AF2_BITS_OF_LAST_BYTE (2)
/** Width in bits of clock drift rate in GPS LNAV subframe 1. */
#define GPS_SUBF1_AF2_FIELD_DIM (8)
/** Start byte of group delay differential in GPS LNAV subframe 1. */
#define GPS_SUBF1_TGD_FIRST_BYTE_IDX (26)
/** End byte of group delay differential in GPS LNAV subframe 1. */
#define GPS_SUBF1_TGD_LAST_BYTE_IDX (27)
/** Start-byte extraction width of group delay differential in GPS LNAV subframe 1. */
#define GPS_SUBF1_TGD_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of group delay differential in GPS LNAV subframe 1. */
#define GPS_SUBF1_TGD_BITS_OF_LAST_BYTE (2)
/** Width in bits of group delay differential in GPS LNAV subframe 1. */
#define GPS_SUBF1_TGD_FIELD_DIM (8)
/** Start byte of ephemeris issue of data in GPS LNAV subframe 2. */
#define GPS_SUBF2_IODE_FIRST_BYTE_IDX (8)
/** End byte of ephemeris issue of data in GPS LNAV subframe 2. */
#define GPS_SUBF2_IODE_LAST_BYTE_IDX (9)
/** Start-byte extraction width of ephemeris issue of data in GPS LNAV subframe 2. */
#define GPS_SUBF2_IODE_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of ephemeris issue of data in GPS LNAV subframe 2. */
#define GPS_SUBF2_IODE_BITS_OF_LAST_BYTE (2)
/** Start byte of sine correction to orbit radius in GPS LNAV subframe 2. */
#define GPS_SUBF2_CRS_FIRST_BYTE_IDX (9)
/** End byte of sine correction to orbit radius in GPS LNAV subframe 2. */
#define GPS_SUBF2_CRS_LAST_BYTE_IDX (11)
/** Start-byte extraction width of sine correction to orbit radius in GPS LNAV subframe 2. */
#define GPS_SUBF2_CRS_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of sine correction to orbit radius in GPS LNAV subframe 2. */
#define GPS_SUBF2_CRS_BITS_OF_LAST_BYTE (2)
/** Width in bits of sine correction to orbit radius in GPS LNAV subframe 2. */
#define GPS_SUBF2_CRS_FIELD_DIM (16)
/** Start byte of mean motion correction in GPS LNAV subframe 2. */
#define GPS_SUBF2_DN_FIRST_BYTE_IDX (12)
/** End byte of mean motion correction in GPS LNAV subframe 2. */
#define GPS_SUBF2_DN_LAST_BYTE_IDX (14)
/** Start-byte extraction width of mean motion correction in GPS LNAV subframe 2. */
#define GPS_SUBF2_DN_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of mean motion correction in GPS LNAV subframe 2. */
#define GPS_SUBF2_DN_BITS_OF_LAST_BYTE (2)
/** Width in bits of mean motion correction in GPS LNAV subframe 2. */
#define GPS_SUBF2_DN_FIELD_DIM (16)
/** Start byte of high part of mean anomaly at reference time in GPS LNAV subframe 2. */
#define GPS_SUBF2_M0_MS_FIRST_BYTE_IDX (14)
/** End byte of high part of mean anomaly at reference time in GPS LNAV subframe 2. */
#define GPS_SUBF2_M0_MS_LAST_BYTE_IDX (15)
/** Start-byte extraction width of high part of mean anomaly at reference time in GPS LNAV subframe 2. */
#define GPS_SUBF2_M0_MS_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of high part of mean anomaly at reference time in GPS LNAV subframe 2. */
#define GPS_SUBF2_M0_MS_BITS_OF_LAST_BYTE (2)
/** Width in bits of high part of mean anomaly at reference time in GPS LNAV subframe 2. */
#define GPS_SUBF2_M0_MS_FIELD_DIM (8)
/** Start byte of low part of mean anomaly at reference time in GPS LNAV subframe 2. */
#define GPS_SUBF2_M0_LS_FIRST_BYTE_IDX (16)
/** End byte of low part of mean anomaly at reference time in GPS LNAV subframe 2. */
#define GPS_SUBF2_M0_LS_LAST_BYTE_IDX (19)
/** Start-byte extraction width of low part of mean anomaly at reference time in GPS LNAV subframe 2. */
#define GPS_SUBF2_M0_LS_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of low part of mean anomaly at reference time in GPS LNAV subframe 2. */
#define GPS_SUBF2_M0_LS_BITS_OF_LAST_BYTE (2)
/** Start byte of cosine correction to argument of latitude in GPS LNAV subframe 2. */
#define GPS_SUBF2_CUC_FIRST_BYTE_IDX (20)
/** End byte of cosine correction to argument of latitude in GPS LNAV subframe 2. */
#define GPS_SUBF2_CUC_LAST_BYTE_IDX (22)
/** Start-byte extraction width of cosine correction to argument of latitude in GPS LNAV subframe 2. */
#define GPS_SUBF2_CUC_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of cosine correction to argument of latitude in GPS LNAV subframe 2. */
#define GPS_SUBF2_CUC_BITS_OF_LAST_BYTE (2)
/** Width in bits of cosine correction to argument of latitude in GPS LNAV subframe 2. */
#define GPS_SUBF2_CUC_FIELD_DIM (16)
/** Start byte of high part of eccentricity in GPS LNAV subframe 2. */
#define GPS_SUBF2_E_MS_FIRST_BYTE_IDX (22)
/** End byte of high part of eccentricity in GPS LNAV subframe 2. */
#define GPS_SUBF2_E_MS_LAST_BYTE_IDX (23)
/** Start-byte extraction width of high part of eccentricity in GPS LNAV subframe 2. */
#define GPS_SUBF2_E_MS_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of high part of eccentricity in GPS LNAV subframe 2. */
#define GPS_SUBF2_E_MS_BITS_OF_LAST_BYTE (2)
/** Start byte of low part of eccentricity in GPS LNAV subframe 2. */
#define GPS_SUBF2_E_LS_FIRST_BYTE_IDX (24)
/** End byte of low part of eccentricity in GPS LNAV subframe 2. */
#define GPS_SUBF2_E_LS_LAST_BYTE_IDX (27)
/** Start-byte extraction width of low part of eccentricity in GPS LNAV subframe 2. */
#define GPS_SUBF2_E_LS_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of low part of eccentricity in GPS LNAV subframe 2. */
#define GPS_SUBF2_E_LS_BITS_OF_LAST_BYTE (2)
/** Start byte of sine correction to argument of latitude in GPS LNAV subframe 2. */
#define GPS_SUBF2_CUS_FIRST_BYTE_IDX (28)
/** End byte of sine correction to argument of latitude in GPS LNAV subframe 2. */
#define GPS_SUBF2_CUS_LAST_BYTE_IDX (30)
/** Start-byte extraction width of sine correction to argument of latitude in GPS LNAV subframe 2. */
#define GPS_SUBF2_CUS_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of sine correction to argument of latitude in GPS LNAV subframe 2. */
#define GPS_SUBF2_CUS_BITS_OF_LAST_BYTE (2)
/** Width in bits of sine correction to argument of latitude in GPS LNAV subframe 2. */
#define GPS_SUBF2_CUS_FIELD_DIM (16)
/** Start byte of high part of square root of semi-major axis in GPS LNAV subframe 2. */
#define GPS_SUBF2_SQRTA_MS_FIRST_BYTE_IDX (30)
/** End byte of high part of square root of semi-major axis in GPS LNAV subframe 2. */
#define GPS_SUBF2_SQRTA_MS_LAST_BYTE_IDX (31)
/** Start-byte extraction width of high part of square root of semi-major axis in GPS LNAV subframe 2. */
#define GPS_SUBF2_SQRTA_MS_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of high part of square root of semi-major axis in GPS LNAV subframe 2. */
#define GPS_SUBF2_SQRTA_MS_BITS_OF_LAST_BYTE (2)
/** Start byte of low part of square root of semi-major axis in GPS LNAV subframe 2. */
#define GPS_SUBF2_SQRTA_LS_FIRST_BYTE_IDX (32)
/** End byte of low part of square root of semi-major axis in GPS LNAV subframe 2. */
#define GPS_SUBF2_SQRTA_LS_LAST_BYTE_IDX (35)
/** Start-byte extraction width of low part of square root of semi-major axis in GPS LNAV subframe 2. */
#define GPS_SUBF2_SQRTA_LS_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of low part of square root of semi-major axis in GPS LNAV subframe 2. */
#define GPS_SUBF2_SQRTA_LS_BITS_OF_LAST_BYTE (2)
/** Start byte of ephemeris reference time in GPS LNAV subframe 2. */
#define GPS_SUBF2_TOE_FIRST_BYTE_IDX (36)
/** End byte of ephemeris reference time in GPS LNAV subframe 2. */
#define GPS_SUBF2_TOE_LAST_BYTE_IDX (38)
/** Start-byte extraction width of ephemeris reference time in GPS LNAV subframe 2. */
#define GPS_SUBF2_TOE_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of ephemeris reference time in GPS LNAV subframe 2. */
#define GPS_SUBF2_TOE_BITS_OF_LAST_BYTE (2)
/** Start byte of cosine correction to inclination in GPS LNAV subframe 3. */
#define GPS_SUBF3_CIC_FIRST_BYTE_IDX (8)
/** End byte of cosine correction to inclination in GPS LNAV subframe 3. */
#define GPS_SUBF3_CIC_LAST_BYTE_IDX (10)
/** Start-byte extraction width of cosine correction to inclination in GPS LNAV subframe 3. */
#define GPS_SUBF3_CIC_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of cosine correction to inclination in GPS LNAV subframe 3. */
#define GPS_SUBF3_CIC_BITS_OF_LAST_BYTE (2)
/** Width in bits of cosine correction to inclination in GPS LNAV subframe 3. */
#define GPS_SUBF3_CIC_FIELD_DIM (16)
/** Start byte of high part of longitude of ascending node at weekly epoch in GPS LNAV subframe 3. */
#define GPS_SUBF3_OMEGA0_MS_FIRST_BYTE_IDX (10)
/** End byte of high part of longitude of ascending node at weekly epoch in GPS LNAV subframe 3. */
#define GPS_SUBF3_OMEGA0_MS_LAST_BYTE_IDX (11)
/** Start-byte extraction width of high part of longitude of ascending node at weekly epoch in GPS LNAV subframe 3. */
#define GPS_SUBF3_OMEGA0_MS_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of high part of longitude of ascending node at weekly epoch in GPS LNAV subframe 3. */
#define GPS_SUBF3_OMEGA0_MS_BITS_OF_LAST_BYTE (2)
/** Width in bits of high part of longitude of ascending node at weekly epoch in GPS LNAV subframe 3. */
#define GPS_SUBF3_OMEGA0_MS_FIELD_DIM (8)
/** Start byte of low part of longitude of ascending node at weekly epoch in GPS LNAV subframe 3. */
#define GPS_SUBF3_OMEGA0_LS_FIRST_BYTE_IDX (12)
/** End byte of low part of longitude of ascending node at weekly epoch in GPS LNAV subframe 3. */
#define GPS_SUBF3_OMEGA0_LS_LAST_BYTE_IDX (15)
/** Start-byte extraction width of low part of longitude of ascending node at weekly epoch in GPS LNAV subframe 3. */
#define GPS_SUBF3_OMEGA0_LS_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of low part of longitude of ascending node at weekly epoch in GPS LNAV subframe 3. */
#define GPS_SUBF3_OMEGA0_LS_BITS_OF_LAST_BYTE (2)
/** Start byte of sine correction to inclination in GPS LNAV subframe 3. */
#define GPS_SUBF3_CIS_FIRST_BYTE_IDX (16)
/** End byte of sine correction to inclination in GPS LNAV subframe 3. */
#define GPS_SUBF3_CIS_LAST_BYTE_IDX (18)
/** Start-byte extraction width of sine correction to inclination in GPS LNAV subframe 3. */
#define GPS_SUBF3_CIS_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of sine correction to inclination in GPS LNAV subframe 3. */
#define GPS_SUBF3_CIS_BITS_OF_LAST_BYTE (2)
/** Width in bits of sine correction to inclination in GPS LNAV subframe 3. */
#define GPS_SUBF3_CIS_FIELD_DIM (16)
/** Start byte of high part of inclination at reference time in GPS LNAV subframe 3. */
#define GPS_SUBF3_I0_MS_FIRST_BYTE_IDX (18)
/** End byte of high part of inclination at reference time in GPS LNAV subframe 3. */
#define GPS_SUBF3_I0_MS_LAST_BYTE_IDX (19)
/** Start-byte extraction width of high part of inclination at reference time in GPS LNAV subframe 3. */
#define GPS_SUBF3_I0_MS_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of high part of inclination at reference time in GPS LNAV subframe 3. */
#define GPS_SUBF3_I0_MS_BITS_OF_LAST_BYTE (2)
/** Width in bits of high part of inclination at reference time in GPS LNAV subframe 3. */
#define GPS_SUBF3_I0_MS_FIELD_DIM (8)
/** Start byte of low part of inclination at reference time in GPS LNAV subframe 3. */
#define GPS_SUBF3_I0_LS_FIRST_BYTE_IDX (20)
/** End byte of low part of inclination at reference time in GPS LNAV subframe 3. */
#define GPS_SUBF3_I0_LS_LAST_BYTE_IDX (23)
/** Start-byte extraction width of low part of inclination at reference time in GPS LNAV subframe 3. */
#define GPS_SUBF3_I0_LS_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of low part of inclination at reference time in GPS LNAV subframe 3. */
#define GPS_SUBF3_I0_LS_BITS_OF_LAST_BYTE (2)
/** Start byte of cosine correction to orbit radius in GPS LNAV subframe 3. */
#define GPS_SUBF3_CRC_FIRST_BYTE_IDX (24)
/** End byte of cosine correction to orbit radius in GPS LNAV subframe 3. */
#define GPS_SUBF3_CRC_LAST_BYTE_IDX (26)
/** Start-byte extraction width of cosine correction to orbit radius in GPS LNAV subframe 3. */
#define GPS_SUBF3_CRC_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of cosine correction to orbit radius in GPS LNAV subframe 3. */
#define GPS_SUBF3_CRC_BITS_OF_LAST_BYTE (2)
/** Width in bits of cosine correction to orbit radius in GPS LNAV subframe 3. */
#define GPS_SUBF3_CRC_FIELD_DIM (16)
/** Start byte of high part of argument of perigee in GPS LNAV subframe 3. */
#define GPS_SUBF3_OMEGA_MS_FIRST_BYTE_IDX (26)
/** End byte of high part of argument of perigee in GPS LNAV subframe 3. */
#define GPS_SUBF3_OMEGA_MS_LAST_BYTE_IDX (27)
/** Start-byte extraction width of high part of argument of perigee in GPS LNAV subframe 3. */
#define GPS_SUBF3_OMEGA_MS_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of high part of argument of perigee in GPS LNAV subframe 3. */
#define GPS_SUBF3_OMEGA_MS_BITS_OF_LAST_BYTE (2)
/** Width in bits of high part of argument of perigee in GPS LNAV subframe 3. */
#define GPS_SUBF3_OMEGA_MS_FIELD_DIM (8)
/** Start byte of low part of argument of perigee in GPS LNAV subframe 3. */
#define GPS_SUBF3_OMEGA_LS_FIRST_BYTE_IDX (28)
/** End byte of low part of argument of perigee in GPS LNAV subframe 3. */
#define GPS_SUBF3_OMEGA_LS_LAST_BYTE_IDX (31)
/** Start-byte extraction width of low part of argument of perigee in GPS LNAV subframe 3. */
#define GPS_SUBF3_OMEGA_LS_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of low part of argument of perigee in GPS LNAV subframe 3. */
#define GPS_SUBF3_OMEGA_LS_BITS_OF_LAST_BYTE (2)
/** Start byte of rate of right ascension in GPS LNAV subframe 3. */
#define GPS_SUBF3_OMEGADOT_FIRST_BYTE_IDX (32)
/** End byte of rate of right ascension in GPS LNAV subframe 3. */
#define GPS_SUBF3_OMEGADOT_LAST_BYTE_IDX (35)
/** Start-byte extraction width of rate of right ascension in GPS LNAV subframe 3. */
#define GPS_SUBF3_OMEGADOT_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of rate of right ascension in GPS LNAV subframe 3. */
#define GPS_SUBF3_OMEGADOT_BITS_OF_LAST_BYTE (2)
/** Width in bits of rate of right ascension in GPS LNAV subframe 3. */
#define GPS_SUBF3_OMEGADOT_FIELD_DIM (24)
/** Start byte of ephemeris issue of data in GPS LNAV subframe 3. */
#define GPS_SUBF3_IODE_FIRST_BYTE_IDX (36)
/** End byte of ephemeris issue of data in GPS LNAV subframe 3. */
#define GPS_SUBF3_IODE_LAST_BYTE_IDX (37)
/** Start-byte extraction width of ephemeris issue of data in GPS LNAV subframe 3. */
#define GPS_SUBF3_IODE_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of ephemeris issue of data in GPS LNAV subframe 3. */
#define GPS_SUBF3_IODE_BITS_OF_LAST_BYTE (2)
/** Start byte of inclination rate in GPS LNAV subframe 3. */
#define GPS_SUBF3_IDOT_FIRST_BYTE_IDX (37)
/** End byte of inclination rate in GPS LNAV subframe 3. */
#define GPS_SUBF3_IDOT_LAST_BYTE_IDX (38)
/** Start-byte extraction width of inclination rate in GPS LNAV subframe 3. */
#define GPS_SUBF3_IDOT_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of inclination rate in GPS LNAV subframe 3. */
#define GPS_SUBF3_IDOT_BITS_OF_LAST_BYTE (8)
/** Width in bits of inclination rate in GPS LNAV subframe 3. */
#define GPS_SUBF3_IDOT_FIELD_DIM (14)
/** Seconds per broadcast unit of Galileo ephemeris reference time. */
#define GAL_TOE_SCALE_FACTOR 60
/** Start byte of time field availability code in Galileo I/NAV word 0. */
#define GAL_TYPE0_TIME_FIRST_BYTE_IDX (0)
/** End byte of time field availability code in Galileo I/NAV word 0. */
#define GAL_TYPE0_TIME_LAST_BYTE_IDX (0)
/** Start-byte extraction width of time field availability code in Galileo I/NAV word 0. */
#define GAL_TYPE0_TIME_BITS_OF_FIRST_BYTE (0)
/** End-byte extraction width of time field availability code in Galileo I/NAV word 0. */
#define GAL_TYPE0_TIME_BITS_OF_LAST_BYTE (2)
/** Start byte of week number in Galileo I/NAV word 0. */
#define GAL_TYPE0_WN_FIRST_BYTE_IDX (12)
/** End byte of week number in Galileo I/NAV word 0. */
#define GAL_TYPE0_WN_LAST_BYTE_IDX (13)
/** Start-byte extraction width of week number in Galileo I/NAV word 0. */
#define GAL_TYPE0_WN_BITS_OF_FIRST_BYTE (8)
/** End-byte extraction width of week number in Galileo I/NAV word 0. */
#define GAL_TYPE0_WN_BITS_OF_LAST_BYTE (4)
/** Start byte of time of week in Galileo I/NAV word 0. */
#define GAL_TYPE0_TOW_FIRST_BYTE_IDX (13)
/** End byte of time of week in Galileo I/NAV word 0. */
#define GAL_TYPE0_TOW_LAST_BYTE_IDX (15)
/** Start-byte extraction width of time of week in Galileo I/NAV word 0. */
#define GAL_TYPE0_TOW_BITS_OF_FIRST_BYTE (4)
/** End-byte extraction width of time of week in Galileo I/NAV word 0. */
#define GAL_TYPE0_TOW_BITS_OF_LAST_BYTE (8)
/** Start byte of navigation issue of data in Galileo I/NAV word 1. */
#define GAL_TYPE1_IOD_FIRST_BYTE_IDX (0)
/** End byte of navigation issue of data in Galileo I/NAV word 1. */
#define GAL_TYPE1_IOD_LAST_BYTE_IDX (1)
/** Start-byte extraction width of navigation issue of data in Galileo I/NAV word 1. */
#define GAL_TYPE1_IOD_BITS_OF_FIRST_BYTE (2)
/** End-byte extraction width of navigation issue of data in Galileo I/NAV word 1. */
#define GAL_TYPE1_IOD_BITS_OF_LAST_BYTE (8)
/** Start byte of mean anomaly at reference time in Galileo I/NAV word 1. */
#define GAL_TYPE1_M0_FIRST_BYTE_IDX (3)
/** End byte of mean anomaly at reference time in Galileo I/NAV word 1. */
#define GAL_TYPE1_M0_LAST_BYTE_IDX (7)
/** Start-byte extraction width of mean anomaly at reference time in Galileo I/NAV word 1. */
#define GAL_TYPE1_M0_BITS_OF_FIRST_BYTE (2)
/** End-byte extraction width of mean anomaly at reference time in Galileo I/NAV word 1. */
#define GAL_TYPE1_M0_BITS_OF_LAST_BYTE (6)
/** Width in bits of mean anomaly at reference time in Galileo I/NAV word 1. */
#define GAL_TYPE1_M0_BITS_FIELD_DIM (32)
/** Start byte of navigation issue of data in Galileo I/NAV word 2. */
#define GAL_TYPE2_IOD_FIRST_BYTE_IDX (0)
/** End byte of navigation issue of data in Galileo I/NAV word 2. */
#define GAL_TYPE2_IOD_LAST_BYTE_IDX (1)
/** Start-byte extraction width of navigation issue of data in Galileo I/NAV word 2. */
#define GAL_TYPE2_IOD_BITS_OF_FIRST_BYTE (2)
/** End-byte extraction width of navigation issue of data in Galileo I/NAV word 2. */
#define GAL_TYPE2_IOD_BITS_OF_LAST_BYTE (8)
/** Start byte of navigation issue of data in Galileo I/NAV word 3. */
#define GAL_TYPE3_IOD_FIRST_BYTE_IDX (0)
/** End byte of navigation issue of data in Galileo I/NAV word 3. */
#define GAL_TYPE3_IOD_LAST_BYTE_IDX (1)
/** Start-byte extraction width of navigation issue of data in Galileo I/NAV word 3. */
#define GAL_TYPE3_IOD_BITS_OF_FIRST_BYTE (2)
/** End-byte extraction width of navigation issue of data in Galileo I/NAV word 3. */
#define GAL_TYPE3_IOD_BITS_OF_LAST_BYTE (8)
/** Start byte of signal-in-space accuracy index in Galileo I/NAV word 3. */
#define GAL_TYPE3_SISA_FIRST_BYTE_IDX (15)
/** End byte of signal-in-space accuracy index in Galileo I/NAV word 3. */
#define GAL_TYPE3_SISA_LAST_BYTE_IDX (15)
/** Start-byte extraction width of signal-in-space accuracy index in Galileo I/NAV word 3. */
#define GAL_TYPE3_SISA_BITS_OF_FIRST_BYTE (8)
/** End-byte extraction width of signal-in-space accuracy index in Galileo I/NAV word 3. */
#define GAL_TYPE3_SISA_BITS_OF_LAST_BYTE (0)
/** Start byte of navigation issue of data in Galileo I/NAV word 4. */
#define GAL_TYPE4_IOD_FIRST_BYTE_IDX (0)
/** End byte of navigation issue of data in Galileo I/NAV word 4. */
#define GAL_TYPE4_IOD_LAST_BYTE_IDX (1)
/** Start-byte extraction width of navigation issue of data in Galileo I/NAV word 4. */
#define GAL_TYPE4_IOD_BITS_OF_FIRST_BYTE (2)
/** End-byte extraction width of navigation issue of data in Galileo I/NAV word 4. */
#define GAL_TYPE4_IOD_BITS_OF_LAST_BYTE (8)
/** Start byte of constant effective ionisation coefficient in Galileo I/NAV word 5. */
#define GAL_TYPE5_A0I_FIRST_BYTE_IDX (0)
/** End byte of constant effective ionisation coefficient in Galileo I/NAV word 5. */
#define GAL_TYPE5_A0I_LAST_BYTE_IDX (2)
/** Start-byte extraction width of constant effective ionisation coefficient in Galileo I/NAV word 5. */
#define GAL_TYPE5_A0I_BITS_OF_FIRST_BYTE (2)
/** End-byte extraction width of constant effective ionisation coefficient in Galileo I/NAV word 5. */
#define GAL_TYPE5_A0I_BITS_OF_LAST_BYTE (1)
/** Start byte of linear effective ionisation coefficient in Galileo I/NAV word 5. */
#define GAL_TYPE5_A1I_FIRST_BYTE_IDX (2)
/** End byte of linear effective ionisation coefficient in Galileo I/NAV word 5. */
#define GAL_TYPE5_A1I_LAST_BYTE_IDX (3)
/** Start-byte extraction width of linear effective ionisation coefficient in Galileo I/NAV word 5. */
#define GAL_TYPE5_A1I_BITS_OF_FIRST_BYTE (7)
/** End-byte extraction width of linear effective ionisation coefficient in Galileo I/NAV word 5. */
#define GAL_TYPE5_A1I_BITS_OF_LAST_BYTE (4)
/** Width in bits of linear effective ionisation coefficient in Galileo I/NAV word 5. */
#define GAL_TYPE5_A1I_BITS_FIELD_DIM (11)
/** Start byte of quadratic effective ionisation coefficient in Galileo I/NAV word 5. */
#define GAL_TYPE5_A2I_FIRST_BYTE_IDX (3)
/** End byte of quadratic effective ionisation coefficient in Galileo I/NAV word 5. */
#define GAL_TYPE5_A2I_LAST_BYTE_IDX (5)
/** Start-byte extraction width of quadratic effective ionisation coefficient in Galileo I/NAV word 5. */
#define GAL_TYPE5_A2I_BITS_OF_FIRST_BYTE (4)
/** End-byte extraction width of quadratic effective ionisation coefficient in Galileo I/NAV word 5. */
#define GAL_TYPE5_A2I_BITS_OF_LAST_BYTE (2)
/** Width in bits of quadratic effective ionisation coefficient in Galileo I/NAV word 5. */
#define GAL_TYPE5_A2I_BITS_FIELD_DIM (14)
/** Start byte of ionospheric disturbance flag for region 1 in Galileo I/NAV word 5. */
#define GAL_TYPE5_SF1_FIRST_BYTE_IDX (5)
/** End byte of ionospheric disturbance flag for region 1 in Galileo I/NAV word 5. */
#define GAL_TYPE5_SF1_LAST_BYTE_IDX (5)
/** Start-byte extraction width of ionospheric disturbance flag for region 1 in Galileo I/NAV word 5. */
#define GAL_TYPE5_SF1_BITS_OF_FIRST_BYTE (3)
/** End-byte extraction width of ionospheric disturbance flag for region 1 in Galileo I/NAV word 5. */
#define GAL_TYPE5_SF1_BITS_OF_LAST_BYTE (6)
/** Start byte of ionospheric disturbance flag for region 2 in Galileo I/NAV word 5. */
#define GAL_TYPE5_SF2_FIRST_BYTE_IDX (5)
/** End byte of ionospheric disturbance flag for region 2 in Galileo I/NAV word 5. */
#define GAL_TYPE5_SF2_LAST_BYTE_IDX (5)
/** Start-byte extraction width of ionospheric disturbance flag for region 2 in Galileo I/NAV word 5. */
#define GAL_TYPE5_SF2_BITS_OF_FIRST_BYTE (4)
/** End-byte extraction width of ionospheric disturbance flag for region 2 in Galileo I/NAV word 5. */
#define GAL_TYPE5_SF2_BITS_OF_LAST_BYTE (5)
/** Start byte of ionospheric disturbance flag for region 3 in Galileo I/NAV word 5. */
#define GAL_TYPE5_SF3_FIRST_BYTE_IDX (5)
/** End byte of ionospheric disturbance flag for region 3 in Galileo I/NAV word 5. */
#define GAL_TYPE5_SF3_LAST_BYTE_IDX (5)
/** Start-byte extraction width of ionospheric disturbance flag for region 3 in Galileo I/NAV word 5. */
#define GAL_TYPE5_SF3_BITS_OF_FIRST_BYTE (5)
/** End-byte extraction width of ionospheric disturbance flag for region 3 in Galileo I/NAV word 5. */
#define GAL_TYPE5_SF3_BITS_OF_LAST_BYTE (4)
/** Start byte of ionospheric disturbance flag for region 4 in Galileo I/NAV word 5. */
#define GAL_TYPE5_SF4_FIRST_BYTE_IDX (5)
/** End byte of ionospheric disturbance flag for region 4 in Galileo I/NAV word 5. */
#define GAL_TYPE5_SF4_LAST_BYTE_IDX (5)
/** Start-byte extraction width of ionospheric disturbance flag for region 4 in Galileo I/NAV word 5. */
#define GAL_TYPE5_SF4_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of ionospheric disturbance flag for region 4 in Galileo I/NAV word 5. */
#define GAL_TYPE5_SF4_BITS_OF_LAST_BYTE (3)
/** Start byte of ionospheric disturbance flag for region 5 in Galileo I/NAV word 5. */
#define GAL_TYPE5_SF5_FIRST_BYTE_IDX (5)
/** End byte of ionospheric disturbance flag for region 5 in Galileo I/NAV word 5. */
#define GAL_TYPE5_SF5_LAST_BYTE_IDX (5)
/** Start-byte extraction width of ionospheric disturbance flag for region 5 in Galileo I/NAV word 5. */
#define GAL_TYPE5_SF5_BITS_OF_FIRST_BYTE (7)
/** End-byte extraction width of ionospheric disturbance flag for region 5 in Galileo I/NAV word 5. */
#define GAL_TYPE5_SF5_BITS_OF_LAST_BYTE (2)
/** Start byte of E1/E5a broadcast group delay in Galileo I/NAV word 5. */
#define GAL_TYPE5_TGD_E1E5A_FIRST_BYTE_IDX (5)
/** End byte of E1/E5a broadcast group delay in Galileo I/NAV word 5. */
#define GAL_TYPE5_TGD_E1E5A_LAST_BYTE_IDX (7)
/** Start-byte extraction width of E1/E5a broadcast group delay in Galileo I/NAV word 5. */
#define GAL_TYPE5_TGD_E1E5A_BITS_OF_FIRST_BYTE (1)
/** End-byte extraction width of E1/E5a broadcast group delay in Galileo I/NAV word 5. */
#define GAL_TYPE5_TGD_E1E5A_BITS_OF_LAST_BYTE (1)
/** Width in bits of E1/E5a broadcast group delay in Galileo I/NAV word 5. */
#define GAL_TYPE5_TGD_E1E5A_BITS_FIELD_DIM (10)
/** Start byte of E1/E5b broadcast group delay in Galileo I/NAV word 5. */
#define GAL_TYPE5_TGD_E1E5B_FIRST_BYTE_IDX (7)
/** End byte of E1/E5b broadcast group delay in Galileo I/NAV word 5. */
#define GAL_TYPE5_TGD_E1E5B_LAST_BYTE_IDX (8)
/** Start-byte extraction width of E1/E5b broadcast group delay in Galileo I/NAV word 5. */
#define GAL_TYPE5_TGD_E1E5B_BITS_OF_FIRST_BYTE (7)
/** End-byte extraction width of E1/E5b broadcast group delay in Galileo I/NAV word 5. */
#define GAL_TYPE5_TGD_E1E5B_BITS_OF_LAST_BYTE (3)
/** Width in bits of E1/E5b broadcast group delay in Galileo I/NAV word 5. */
#define GAL_TYPE5_TGD_E1E5B_BITS_FIELD_DIM (10)
/** Start byte of week number in Galileo I/NAV word 5. */
#define GAL_TYPE5_WN_FIRST_BYTE_IDX (9)
/** End byte of week number in Galileo I/NAV word 5. */
#define GAL_TYPE5_WN_LAST_BYTE_IDX (10)
/** Start-byte extraction width of week number in Galileo I/NAV word 5. */
#define GAL_TYPE5_WN_BITS_OF_FIRST_BYTE (7)
/** End-byte extraction width of week number in Galileo I/NAV word 5. */
#define GAL_TYPE5_WN_BITS_OF_LAST_BYTE (5)
/** Start byte of time of week in Galileo I/NAV word 5. */
#define GAL_TYPE5_TOW_FIRST_BYTE_IDX (10)
/** End byte of time of week in Galileo I/NAV word 5. */
#define GAL_TYPE5_TOW_LAST_BYTE_IDX (13)
/** Start-byte extraction width of time of week in Galileo I/NAV word 5. */
#define GAL_TYPE5_TOW_BITS_OF_FIRST_BYTE (3)
/** End-byte extraction width of time of week in Galileo I/NAV word 5. */
#define GAL_TYPE5_TOW_BITS_OF_LAST_BYTE (1)
/** Start byte of E1-B signal health in Galileo I/NAV word 5. */
#define GAL_TYPE5_E1BHS_FIRST_BYTE_IDX (8)
/** End byte of E1-B signal health in Galileo I/NAV word 5. */
#define GAL_TYPE5_E1BHS_LAST_BYTE_IDX (8)
/** Start-byte extraction width of E1-B signal health in Galileo I/NAV word 5. */
#define GAL_TYPE5_E1BHS_BITS_OF_FIRST_BYTE (7)
/** End-byte extraction width of E1-B signal health in Galileo I/NAV word 5. */
#define GAL_TYPE5_E1BHS_BITS_OF_LAST_BYTE (3)
/** Start byte of E1-B data validity in Galileo I/NAV word 5. */
#define GAL_TYPE5_E1BDVS_FIRST_BYTE_IDX (9)
/** End byte of E1-B data validity in Galileo I/NAV word 5. */
#define GAL_TYPE5_E1BDVS_LAST_BYTE_IDX (9)
/** Start-byte extraction width of E1-B data validity in Galileo I/NAV word 5. */
#define GAL_TYPE5_E1BDVS_BITS_OF_FIRST_BYTE (1)
/** End-byte extraction width of E1-B data validity in Galileo I/NAV word 5. */
#define GAL_TYPE5_E1BDVS_BITS_OF_LAST_BYTE (0)
/** Number of transmitted symbols in a nominal Galileo I/NAV page. */
#define GAL_PAGE_LENGTH 500
/** Broadcast scaling factor \f$2^{-31}\f$. */
#define POW_TWO_M31 4.656612873077393e-10
/** Broadcast scaling factor \f$2^{-43}\f$. */
#define POW_TWO_M43 1.1368683772161603e-13
/** Broadcast scaling factor \f$2^{-55}\f$. */
#define POW_TWO_M55 2.7755575615628914e-17
/** Broadcast scaling factor \f$2^{-5}\f$. */
#define POW_TWO_M05 0.03125
/** Broadcast scaling factor \f$2^{24}\f$. */
#define POW_TWO_P24 16777216
/** Broadcast scaling factor \f$2^{-29}\f$. */
#define POW_TWO_M29 1.862645149230957e-9
/** Broadcast scaling factor \f$2^{-33}\f$. */
#define POW_TWO_M33 1.1641532182693481e-10
/** Broadcast scaling factor \f$2^{-19}\f$. */
#define POW_TWO_M19 0.0000019073486328125
/** Broadcast scaling factor \f$2^{-34}\f$. */
#define POW_TWO_M34 5.820766091346741e-11
/** Broadcast scaling factor \f$2^{-46}\f$. */
#define POW_TWO_M46 1.4210854715202004e-14
/** Broadcast scaling factor \f$2^{-59}\f$. */
#define POW_TWO_M59 1.734723475976807e-18
/** Broadcast scaling factor \f$2^{-2}\f$. */
#define POW_TWO_M02 0.25
/** Broadcast scaling factor \f$2^{-8}\f$. */
#define POW_TWO_M08 0.00390625
/** Broadcast scaling factor \f$2^{-15}\f$. */
#define POW_TWO_M15 0.000030517578125
/** Broadcast scaling factor \f$2^{-32}\f$. */
#define POW_TWO_M32 2.3283064365386963e-10
/** Start byte of ephemeris reference time in Galileo I/NAV word 1. */
#define GAL_TYPE1_TOE_FIRST_BYTE_IDX (2)
/** End byte of ephemeris reference time in Galileo I/NAV word 1. */
#define GAL_TYPE1_TOE_LAST_BYTE_IDX (3)
/** Start-byte extraction width of ephemeris reference time in Galileo I/NAV word 1. */
#define GAL_TYPE1_TOE_BITS_OF_FIRST_BYTE (8)
/** End-byte extraction width of ephemeris reference time in Galileo I/NAV word 1. */
#define GAL_TYPE1_TOE_BITS_OF_LAST_BYTE (6)
/** Start byte of eccentricity in Galileo I/NAV word 1. */
#define GAL_TYPE1_E_FIRST_BYTE_IDX (7)
/** End byte of eccentricity in Galileo I/NAV word 1. */
#define GAL_TYPE1_E_LAST_BYTE_IDX (11)
/** Start-byte extraction width of eccentricity in Galileo I/NAV word 1. */
#define GAL_TYPE1_E_BITS_OF_FIRST_BYTE (2)
/** End-byte extraction width of eccentricity in Galileo I/NAV word 1. */
#define GAL_TYPE1_E_BITS_OF_LAST_BYTE (6)
/** Start byte of square root of semi-major axis in Galileo I/NAV word 1. */
#define GAL_TYPE1_SQRTA_FIRST_BYTE_IDX (11)
/** End byte of square root of semi-major axis in Galileo I/NAV word 1. */
#define GAL_TYPE1_SQRTA_LAST_BYTE_IDX (15)
/** Start-byte extraction width of square root of semi-major axis in Galileo I/NAV word 1. */
#define GAL_TYPE1_SQRTA_BITS_OF_FIRST_BYTE (2)
/** End-byte extraction width of square root of semi-major axis in Galileo I/NAV word 1. */
#define GAL_TYPE1_SQRTA_BITS_OF_LAST_BYTE (6)
/** Start byte of longitude of ascending node at weekly epoch in Galileo I/NAV word 2. */
#define GAL_TYPE2_OMEGA0_FIRST_BYTE_IDX (2)
/** End byte of longitude of ascending node at weekly epoch in Galileo I/NAV word 2. */
#define GAL_TYPE2_OMEGA0_LAST_BYTE_IDX (5)
/** Start-byte extraction width of longitude of ascending node at weekly epoch in Galileo I/NAV word 2. */
#define GAL_TYPE2_OMEGA0_BITS_OF_FIRST_BYTE (8)
/** End-byte extraction width of longitude of ascending node at weekly epoch in Galileo I/NAV word 2. */
#define GAL_TYPE2_OMEGA0_BITS_OF_LAST_BYTE (8)
/** Width in bits of longitude of ascending node at weekly epoch in Galileo I/NAV word 2. */
#define GAL_TYPE2_OMEGA0_BITS_FIELD_DIM (32)
/** Start byte of inclination at reference time in Galileo I/NAV word 2. */
#define GAL_TYPE2_I0_FIRST_BYTE_IDX (6)
/** End byte of inclination at reference time in Galileo I/NAV word 2. */
#define GAL_TYPE2_I0_LAST_BYTE_IDX (9)
/** Start-byte extraction width of inclination at reference time in Galileo I/NAV word 2. */
#define GAL_TYPE2_I0_BITS_OF_FIRST_BYTE (8)
/** End-byte extraction width of inclination at reference time in Galileo I/NAV word 2. */
#define GAL_TYPE2_I0_BITS_OF_LAST_BYTE (8)
/** Width in bits of inclination at reference time in Galileo I/NAV word 2. */
#define GAL_TYPE2_I0_BITS_FIELD_DIM (32)
/** Start byte of argument of perigee in Galileo I/NAV word 2. */
#define GAL_TYPE2_OMEGA_FIRST_BYTE_IDX (10)
/** End byte of argument of perigee in Galileo I/NAV word 2. */
#define GAL_TYPE2_OMEGA_LAST_BYTE_IDX (13)
/** Start-byte extraction width of argument of perigee in Galileo I/NAV word 2. */
#define GAL_TYPE2_OMEGA_BITS_OF_FIRST_BYTE (8)
/** End-byte extraction width of argument of perigee in Galileo I/NAV word 2. */
#define GAL_TYPE2_OMEGA_BITS_OF_LAST_BYTE (8)
/** Width in bits of argument of perigee in Galileo I/NAV word 2. */
#define GAL_TYPE2_OMEGA_BITS_FIELD_DIM (32)
/** Start byte of inclination rate in Galileo I/NAV word 2. */
#define GAL_TYPE2_IDOT_FIRST_BYTE_IDX (14)
/** End byte of inclination rate in Galileo I/NAV word 2. */
#define GAL_TYPE2_IDOT_LAST_BYTE_IDX (15)
/** Start-byte extraction width of inclination rate in Galileo I/NAV word 2. */
#define GAL_TYPE2_IDOT_BITS_OF_FIRST_BYTE (8)
/** End-byte extraction width of inclination rate in Galileo I/NAV word 2. */
#define GAL_TYPE2_IDOT_BITS_OF_LAST_BYTE (6)
/** Width in bits of inclination rate in Galileo I/NAV word 2. */
#define GAL_TYPE2_IDOT_BITS_FIELD_DIM (14)
/** Start byte of rate of right ascension in Galileo I/NAV word 3. */
#define GAL_TYPE3_OMEGADOT_FIRST_BYTE_IDX (2)
/** End byte of rate of right ascension in Galileo I/NAV word 3. */
#define GAL_TYPE3_OMEGADOT_LAST_BYTE_IDX (4)
/** Start-byte extraction width of rate of right ascension in Galileo I/NAV word 3. */
#define GAL_TYPE3_OMEGADOT_BITS_OF_FIRST_BYTE (8)
/** End-byte extraction width of rate of right ascension in Galileo I/NAV word 3. */
#define GAL_TYPE3_OMEGADOT_BITS_OF_LAST_BYTE (8)
/** Width in bits of rate of right ascension in Galileo I/NAV word 3. */
#define GAL_TYPE3_OMEGADOT_BITS_FIELD_DIM (24)
/** Start byte of mean motion correction in Galileo I/NAV word 3. */
#define GAL_TYPE3_DELTAN_FIRST_BYTE_IDX (5)
/** End byte of mean motion correction in Galileo I/NAV word 3. */
#define GAL_TYPE3_DELTAN_LAST_BYTE_IDX (6)
/** Start-byte extraction width of mean motion correction in Galileo I/NAV word 3. */
#define GAL_TYPE3_DELTAN_BITS_OF_FIRST_BYTE (8)
/** End-byte extraction width of mean motion correction in Galileo I/NAV word 3. */
#define GAL_TYPE3_DELTAN_BITS_OF_LAST_BYTE (8)
/** Width in bits of mean motion correction in Galileo I/NAV word 3. */
#define GAL_TYPE3_DELTAN_BITS_FIELD_DIM (16)
/** Start byte of cosine correction to argument of latitude in Galileo I/NAV word 3. */
#define GAL_TYPE3_CUC_FIRST_BYTE_IDX (7)
/** End byte of cosine correction to argument of latitude in Galileo I/NAV word 3. */
#define GAL_TYPE3_CUC_LAST_BYTE_IDX (8)
/** Start-byte extraction width of cosine correction to argument of latitude in Galileo I/NAV word 3. */
#define GAL_TYPE3_CUC_BITS_OF_FIRST_BYTE (8)
/** End-byte extraction width of cosine correction to argument of latitude in Galileo I/NAV word 3. */
#define GAL_TYPE3_CUC_BITS_OF_LAST_BYTE (8)
/** Width in bits of cosine correction to argument of latitude in Galileo I/NAV word 3. */
#define GAL_TYPE3_CUC_BITS_FIELD_DIM (16)
/** Start byte of sine correction to argument of latitude in Galileo I/NAV word 3. */
#define GAL_TYPE3_CUS_FIRST_BYTE_IDX (9)
/** End byte of sine correction to argument of latitude in Galileo I/NAV word 3. */
#define GAL_TYPE3_CUS_LAST_BYTE_IDX (10)
/** Start-byte extraction width of sine correction to argument of latitude in Galileo I/NAV word 3. */
#define GAL_TYPE3_CUS_BITS_OF_FIRST_BYTE (8)
/** End-byte extraction width of sine correction to argument of latitude in Galileo I/NAV word 3. */
#define GAL_TYPE3_CUS_BITS_OF_LAST_BYTE (8)
/** Width in bits of sine correction to argument of latitude in Galileo I/NAV word 3. */
#define GAL_TYPE3_CUS_BITS_FIELD_DIM (16)
/** Start byte of cosine correction to orbit radius in Galileo I/NAV word 3. */
#define GAL_TYPE3_CRC_FIRST_BYTE_IDX (11)
/** End byte of cosine correction to orbit radius in Galileo I/NAV word 3. */
#define GAL_TYPE3_CRC_LAST_BYTE_IDX (12)
/** Start-byte extraction width of cosine correction to orbit radius in Galileo I/NAV word 3. */
#define GAL_TYPE3_CRC_BITS_OF_FIRST_BYTE (8)
/** End-byte extraction width of cosine correction to orbit radius in Galileo I/NAV word 3. */
#define GAL_TYPE3_CRC_BITS_OF_LAST_BYTE (8)
/** Width in bits of cosine correction to orbit radius in Galileo I/NAV word 3. */
#define GAL_TYPE3_CRC_BITS_FIELD_DIM (16)
/** Start byte of sine correction to orbit radius in Galileo I/NAV word 3. */
#define GAL_TYPE3_CRS_FIRST_BYTE_IDX (13)
/** End byte of sine correction to orbit radius in Galileo I/NAV word 3. */
#define GAL_TYPE3_CRS_LAST_BYTE_IDX (14)
/** Start-byte extraction width of sine correction to orbit radius in Galileo I/NAV word 3. */
#define GAL_TYPE3_CRS_BITS_OF_FIRST_BYTE (8)
/** End-byte extraction width of sine correction to orbit radius in Galileo I/NAV word 3. */
#define GAL_TYPE3_CRS_BITS_OF_LAST_BYTE (8)
/** Width in bits of sine correction to orbit radius in Galileo I/NAV word 3. */
#define GAL_TYPE3_CRS_BITS_FIELD_DIM (16)
/** Start byte of satellite identifier in Galileo I/NAV word 4. */
#define GAL_TYPE4_PRN_FIRST_BYTE_IDX (2)
/** End byte of satellite identifier in Galileo I/NAV word 4. */
#define GAL_TYPE4_PRN_LAST_BYTE_IDX (2)
/** Start-byte extraction width of satellite identifier in Galileo I/NAV word 4. */
#define GAL_TYPE4_PRN_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of satellite identifier in Galileo I/NAV word 4. */
#define GAL_TYPE4_PRN_BITS_OF_LAST_BYTE (0)
/** Start byte of cosine correction to inclination in Galileo I/NAV word 4. */
#define GAL_TYPE4_CIC_FIRST_BYTE_IDX (2)
/** End byte of cosine correction to inclination in Galileo I/NAV word 4. */
#define GAL_TYPE4_CIC_LAST_BYTE_IDX (4)
/** Start-byte extraction width of cosine correction to inclination in Galileo I/NAV word 4. */
#define GAL_TYPE4_CIC_BITS_OF_FIRST_BYTE (2)
/** End-byte extraction width of cosine correction to inclination in Galileo I/NAV word 4. */
#define GAL_TYPE4_CIC_BITS_OF_LAST_BYTE (6)
/** Width in bits of cosine correction to inclination in Galileo I/NAV word 4. */
#define GAL_TYPE4_CIC_BITS_FIELD_DIM (16)
/** Start byte of sine correction to inclination in Galileo I/NAV word 4. */
#define GAL_TYPE4_CIS_FIRST_BYTE_IDX (4)
/** End byte of sine correction to inclination in Galileo I/NAV word 4. */
#define GAL_TYPE4_CIS_LAST_BYTE_IDX (6)
/** Start-byte extraction width of sine correction to inclination in Galileo I/NAV word 4. */
#define GAL_TYPE4_CIS_BITS_OF_FIRST_BYTE (2)
/** End-byte extraction width of sine correction to inclination in Galileo I/NAV word 4. */
#define GAL_TYPE4_CIS_BITS_OF_LAST_BYTE (6)
/** Width in bits of sine correction to inclination in Galileo I/NAV word 4. */
#define GAL_TYPE4_CIS_BITS_FIELD_DIM (16)
/** Start byte of clock reference time in Galileo I/NAV word 4. */
#define GAL_TYPE4_COC_FIRST_BYTE_IDX (6)
/** End byte of clock reference time in Galileo I/NAV word 4. */
#define GAL_TYPE4_COC_LAST_BYTE_IDX (8)
/** Start-byte extraction width of clock reference time in Galileo I/NAV word 4. */
#define GAL_TYPE4_COC_BITS_OF_FIRST_BYTE (2)
/** End-byte extraction width of clock reference time in Galileo I/NAV word 4. */
#define GAL_TYPE4_COC_BITS_OF_LAST_BYTE (4)
/** Start byte of clock bias in Galileo I/NAV word 4. */
#define GAL_TYPE4_AF0_FIRST_BYTE_IDX (8)
/** End byte of clock bias in Galileo I/NAV word 4. */
#define GAL_TYPE4_AF0_LAST_BYTE_IDX (12)
/** Start-byte extraction width of clock bias in Galileo I/NAV word 4. */
#define GAL_TYPE4_AF0_BITS_OF_FIRST_BYTE (4)
/** End-byte extraction width of clock bias in Galileo I/NAV word 4. */
#define GAL_TYPE4_AF0_BITS_OF_LAST_BYTE (3)
/** Width in bits of clock bias in Galileo I/NAV word 4. */
#define GAL_TYPE4_AF0_BITS_FIELD_DIM (31)
/** Start byte of clock drift in Galileo I/NAV word 4. */
#define GAL_TYPE4_AF1_FIRST_BYTE_IDX (12)
/** End byte of clock drift in Galileo I/NAV word 4. */
#define GAL_TYPE4_AF1_LAST_BYTE_IDX (14)
/** Start-byte extraction width of clock drift in Galileo I/NAV word 4. */
#define GAL_TYPE4_AF1_BITS_OF_FIRST_BYTE (5)
/** End-byte extraction width of clock drift in Galileo I/NAV word 4. */
#define GAL_TYPE4_AF1_BITS_OF_LAST_BYTE (8)
/** Width in bits of clock drift in Galileo I/NAV word 4. */
#define GAL_TYPE4_AF1_BITS_FIELD_DIM (21)
/** Start byte of clock drift rate in Galileo I/NAV word 4. */
#define GAL_TYPE4_AF2_FIRST_BYTE_IDX (15)
/** End byte of clock drift rate in Galileo I/NAV word 4. */
#define GAL_TYPE4_AF2_LAST_BYTE_IDX (15)
/** Start-byte extraction width of clock drift rate in Galileo I/NAV word 4. */
#define GAL_TYPE4_AF2_BITS_OF_FIRST_BYTE (6)
/** End-byte extraction width of clock drift rate in Galileo I/NAV word 4. */
#define GAL_TYPE4_AF2_BITS_OF_LAST_BYTE (0)
/** Width in bits of clock drift rate in Galileo I/NAV word 4. */
#define GAL_TYPE4_AF2_BITS_FIELD_DIM (6)

/** Local view over the already validated payload. */
typedef struct {
    TNgBroadcast *navMsg;           /**< Borrowed channel-local navigation cache. */
    unsigned char subframe[40];     /**< Ten GPS words, including parity gaps, in wire order. */
    unsigned char bitsForParse[16]; /**< Validated Galileo I/NAV word in wire order. */
    int wnCorrection;               /**< Week-number offset, initialized to zero by the caller. */
    int bitIndexTotal;              /**< Nominal Galileo page length recorded after time decode. */
} TNgNavParser;

/** Apply the GPS LNAV satellite summary bit and component availability code to L1 C/A.
 * @param health Six-bit satellite health field from GPS LNAV subframe 1.
 * @return Nonzero if GPS L1 C/A may be used with this health indication.
 */
static int NgGpsL1CaHealthy(unsigned health)
{
    unsigned signal = health & 31;
    return !(health & 32) && signal != 2 && signal != 3 && signal != 23 && signal != 24 &&
           signal != 28 && signal != 31;
}

/** Extract a signed, packed navigation field. First concatenate its bits into the
 * unsigned integer \f$u\f$. For a field of width \f$n\f$, return
 * \f$u-2^n\f$ when \f$u\geq2^{n-1}\f$, and \f$u\f$ otherwise.
 *
 * @param dataBytes Packed navigation payload in transmission order.
 * @param startByte Zero-based index of the high end of the field.
 * @param endByte Zero-based index of the low end of the field.
 * @param bitsOfFirstByte Number of field bits selected from the first byte.
 * @param bitsOfLastByte Number of field bits selected from the last byte.
 * @param fieldBitCount Encoded width in bits, including the sign bit.
 * @return Signed integer value prior to applying the broadcast scale.
 */
static double NgExtractTwoCompField(unsigned char dataBytes[], int startByte, int endByte,
                                    int bitsOfFirstByte, int bitsOfLastByte, int fieldBitCount)
{
    int firstByteShift;
    int lastByteShift;
    unsigned int shiftedBits;
    double fieldValue;
    firstByteShift = 8 - bitsOfFirstByte;
    lastByteShift = 8 - bitsOfLastByte;
    if (startByte == endByte) {
        if (firstByteShift == 8) {
            shiftedBits = dataBytes[startByte];
            shiftedBits = (shiftedBits << (24 + lastByteShift)) >> (24 + lastByteShift);
            fieldValue = shiftedBits;
        } else {
            fieldValue = dataBytes[startByte] >> firstByteShift;
        }
    } else {
        shiftedBits = dataBytes[startByte];
        if (firstByteShift != 0) {
            shiftedBits = (shiftedBits << (24 + firstByteShift)) >> (24 + firstByteShift);
        }
        shiftedBits = (shiftedBits << ((8 * (endByte - startByte)) - lastByteShift));
        fieldValue = shiftedBits;
        int byteIndex;
        for (byteIndex = startByte + 1; byteIndex < endByte; byteIndex++) {
            shiftedBits = dataBytes[byteIndex];
            shiftedBits = (shiftedBits << ((8 * (endByte - byteIndex)) - lastByteShift));
            fieldValue += shiftedBits;
        }
        shiftedBits = dataBytes[endByte];
        shiftedBits = shiftedBits >> lastByteShift;
        fieldValue += shiftedBits;
    }
    if ((ldexp(1.0, fieldBitCount - 1) <= fieldValue) &&
        (fieldValue <= ldexp(1.0, fieldBitCount) - 1)) {
        fieldValue = fieldValue - ldexp(1.0, fieldBitCount);
    }
    return fieldValue;
}

/** Extract an unsigned navigation field from packed bytes. A multi-byte field
 * takes the low bits of its first byte, complete intervening bytes, and the high
 * bits of its last byte. For an in-byte field, select the low or high slice using
 * the widths in the field map.
 *
 * @param dataBytes Packed navigation payload in transmission order.
 * @param startByte Zero-based index of the first byte containing the field.
 * @param endByte Zero-based index of the last byte containing the field.
 * @param bitsOfFirstByte Start-byte width, or zero for an in-byte low slice.
 * @param bitsOfLastByte End-byte width used by the field map.
 * @return Unsigned integer value prior to applying the broadcast scale.
 */
static double NgExtractField(unsigned char dataBytes[], int startByte, int endByte,
                             int bitsOfFirstByte, int bitsOfLastByte)
{
    int firstByteShift;
    int lastByteShift;
    unsigned int shiftedBits;
    double fieldValue;
    firstByteShift = 8 - bitsOfFirstByte;
    lastByteShift = 8 - bitsOfLastByte;
    if (startByte == endByte) {
        if (firstByteShift == 8) {
            shiftedBits = dataBytes[startByte];
            shiftedBits = (shiftedBits << (24 + lastByteShift)) >> (24 + lastByteShift);
            fieldValue = shiftedBits;
        } else {
            shiftedBits = (unsigned int)(dataBytes[startByte] >> firstByteShift);
            if (lastByteShift < 8) {
                shiftedBits = (shiftedBits << (24 + lastByteShift + firstByteShift)) >>
                              (24 + lastByteShift + firstByteShift);
            }
            fieldValue = shiftedBits;
        }
    } else {
        shiftedBits = dataBytes[startByte];
        if (firstByteShift != 0) {
            shiftedBits = (shiftedBits << (24 + firstByteShift)) >> (24 + firstByteShift);
        }
        shiftedBits = (shiftedBits << ((8 * (endByte - startByte)) - lastByteShift));
        fieldValue = shiftedBits;
        int byteIndex;
        for (byteIndex = startByte + 1; byteIndex < endByte; byteIndex++) {
            shiftedBits = dataBytes[byteIndex];
            shiftedBits = (shiftedBits << ((8 * (endByte - byteIndex)) - lastByteShift));
            fieldValue += shiftedBits;
        }
        shiftedBits = dataBytes[endByte];
        shiftedBits = shiftedBits >> lastByteShift;
        fieldValue += shiftedBits;
    }
    return fieldValue;
}

/** Decode clock coefficients, health and reference time from GPS LNAV subframe 1.
 * @param parser Channel-local parse context with the validated payload and staging state.
 * @return Parser status. A rejected GPS segment returns a negative value.
 */
static int NgParseGpsSubframe1(TNgNavParser *parser)
{
    TNgBroadcast *navMsg = parser->navMsg;
    int health;
    TNgOrbit *eph = &(navMsg->ephTmp);
    TNgBroadcastClock *clk = &(navMsg->clkTmp);
    navMsg->wn = (int)(NgExtractField(parser->subframe, GPS_SUBF1_WN_FIRST_BYTE_IDX,
                                      GPS_SUBF1_WN_LAST_BYTE_IDX, GPS_SUBF1_WN_BITS_OF_FIRST_BYTE,
                                      GPS_SUBF1_WN_BITS_OF_LAST_BYTE));
    navMsg->wn += (int)(parser->wnCorrection);
    navMsg->wnValid = TRUE_FLAG;
    health = (int)(NgExtractField(
        parser->subframe, GPS_SUBF1_HEALTH_FIRST_BYTE_IDX, GPS_SUBF1_HEALTH_LAST_BYTE_IDX,
        GPS_SUBF1_HEALTH_BITS_OF_FIRST_BYTE, GPS_SUBF1_HEALTH_BITS_OF_LAST_BYTE));
    if (NgGpsL1CaHealthy((unsigned)health)) {
        eph->health = (int)(SIGNAL_OK);
    } else {
        eph->health = (int)(SIGNAL_OUT_OF_SERVICE);
    }
    eph->sisa = (int)(NgExtractField(parser->subframe, GPS_SUBF1_URA_FIRST_BYTE_IDX,
                                     GPS_SUBF1_URA_LAST_BYTE_IDX, GPS_SUBF1_URA_BITS_OF_FIRST_BYTE,
                                     GPS_SUBF1_URA_BITS_OF_LAST_BYTE));
    clk->iodc = (int)(256.0 * NgExtractField(parser->subframe, GPS_SUBF1_IODC_MS_FIRST_BYTE_IDX,
                                             GPS_SUBF1_IODC_MS_LAST_BYTE_IDX,
                                             GPS_SUBF1_IODC_MS_BITS_OF_FIRST_BYTE,
                                             GPS_SUBF1_IODC_MS_BITS_OF_LAST_BYTE));
    clk->iodc += (int)(NgExtractField(
        parser->subframe, GPS_SUBF1_IODC_LS_FIRST_BYTE_IDX, GPS_SUBF1_IODC_LS_LAST_BYTE_IDX,
        GPS_SUBF1_IODC_LS_BITS_OF_FIRST_BYTE, GPS_SUBF1_IODC_LS_BITS_OF_LAST_BYTE));
    clk->toc = 16.0 * NgExtractField(parser->subframe, GPS_SUBF1_TOC_FIRST_BYTE_IDX,
                                     GPS_SUBF1_TOC_LAST_BYTE_IDX, GPS_SUBF1_TOC_BITS_OF_FIRST_BYTE,
                                     GPS_SUBF1_TOC_BITS_OF_LAST_BYTE);
    clk->af[0] = POW_TWO_M31 * NgExtractTwoCompField(
                                   parser->subframe, GPS_SUBF1_AF0_FIRST_BYTE_IDX,
                                   GPS_SUBF1_AF0_LAST_BYTE_IDX, GPS_SUBF1_AF0_BITS_OF_FIRST_BYTE,
                                   GPS_SUBF1_AF0_BITS_OF_LAST_BYTE, GPS_SUBF1_AF0_FIELD_DIM);
    clk->af[1] = POW_TWO_M43 * NgExtractTwoCompField(
                                   parser->subframe, GPS_SUBF1_AF1_FIRST_BYTE_IDX,
                                   GPS_SUBF1_AF1_LAST_BYTE_IDX, GPS_SUBF1_AF1_BITS_OF_FIRST_BYTE,
                                   GPS_SUBF1_AF1_BITS_OF_LAST_BYTE, GPS_SUBF1_AF1_FIELD_DIM);
    clk->af[2] = POW_TWO_M55 * NgExtractTwoCompField(
                                   parser->subframe, GPS_SUBF1_AF2_FIRST_BYTE_IDX,
                                   GPS_SUBF1_AF2_LAST_BYTE_IDX, GPS_SUBF1_AF2_BITS_OF_FIRST_BYTE,
                                   GPS_SUBF1_AF2_BITS_OF_LAST_BYTE, GPS_SUBF1_AF2_FIELD_DIM);
    clk->tGroupDelay =
        POW_TWO_M31 *
        NgExtractTwoCompField(parser->subframe, GPS_SUBF1_TGD_FIRST_BYTE_IDX,
                              GPS_SUBF1_TGD_LAST_BYTE_IDX, GPS_SUBF1_TGD_BITS_OF_FIRST_BYTE,
                              GPS_SUBF1_TGD_BITS_OF_LAST_BYTE, GPS_SUBF1_TGD_FIELD_DIM);
    if (clk->toc > GPS_MAXIMUM_TOC) {
        return FAILURE_RETURN;
    }
    navMsg->subframe1Received = TRUE_FLAG;

    return SUCCESS_RETURN;
}

/** Decode the first orbit segment and validate its reference time and eccentricity.
 * @param parser Channel-local parse context with the validated payload and staging state.
 * @return Parser status. A rejected GPS segment returns a negative value.
 */
static int NgParseGpsSubframe2(TNgNavParser *parser)
{
    TNgBroadcast *navMsg = parser->navMsg;
    TNgOrbit *eph = &(navMsg->ephTmp);
    eph->iode2 = (int)(NgExtractField(
        parser->subframe, GPS_SUBF2_IODE_FIRST_BYTE_IDX, GPS_SUBF2_IODE_LAST_BYTE_IDX,
        GPS_SUBF2_IODE_BITS_OF_FIRST_BYTE, GPS_SUBF2_IODE_BITS_OF_LAST_BYTE));
    eph->capitalCrs =
        POW_TWO_M05 *
        NgExtractTwoCompField(parser->subframe, GPS_SUBF2_CRS_FIRST_BYTE_IDX,
                              GPS_SUBF2_CRS_LAST_BYTE_IDX, GPS_SUBF2_CRS_BITS_OF_FIRST_BYTE,
                              GPS_SUBF2_CRS_BITS_OF_LAST_BYTE, GPS_SUBF2_CRS_FIELD_DIM);
    eph->deltaN = POW_TWO_M43 * NG_BROADCAST_PI *
                  NgExtractTwoCompField(parser->subframe, GPS_SUBF2_DN_FIRST_BYTE_IDX,
                                        GPS_SUBF2_DN_LAST_BYTE_IDX, GPS_SUBF2_DN_BITS_OF_FIRST_BYTE,
                                        GPS_SUBF2_DN_BITS_OF_LAST_BYTE, GPS_SUBF2_DN_FIELD_DIM);
    eph->meanAnomaly0 =
        POW_TWO_P24 *
        NgExtractTwoCompField(parser->subframe, GPS_SUBF2_M0_MS_FIRST_BYTE_IDX,
                              GPS_SUBF2_M0_MS_LAST_BYTE_IDX, GPS_SUBF2_M0_MS_BITS_OF_FIRST_BYTE,
                              GPS_SUBF2_M0_MS_BITS_OF_LAST_BYTE, GPS_SUBF2_M0_MS_FIELD_DIM);
    eph->meanAnomaly0 += NgExtractField(
        parser->subframe, GPS_SUBF2_M0_LS_FIRST_BYTE_IDX, GPS_SUBF2_M0_LS_LAST_BYTE_IDX,
        GPS_SUBF2_M0_LS_BITS_OF_FIRST_BYTE, GPS_SUBF2_M0_LS_BITS_OF_LAST_BYTE);
    eph->meanAnomaly0 *= POW_TWO_M31 * NG_BROADCAST_PI;
    eph->capitalCuc =
        POW_TWO_M29 *
        NgExtractTwoCompField(parser->subframe, GPS_SUBF2_CUC_FIRST_BYTE_IDX,
                              GPS_SUBF2_CUC_LAST_BYTE_IDX, GPS_SUBF2_CUC_BITS_OF_FIRST_BYTE,
                              GPS_SUBF2_CUC_BITS_OF_LAST_BYTE, GPS_SUBF2_CUC_FIELD_DIM);
    eph->eccentricity =
        POW_TWO_P24 * NgExtractField(parser->subframe, GPS_SUBF2_E_MS_FIRST_BYTE_IDX,
                                     GPS_SUBF2_E_MS_LAST_BYTE_IDX,
                                     GPS_SUBF2_E_MS_BITS_OF_FIRST_BYTE,
                                     GPS_SUBF2_E_MS_BITS_OF_LAST_BYTE);
    eph->eccentricity += NgExtractField(
        parser->subframe, GPS_SUBF2_E_LS_FIRST_BYTE_IDX, GPS_SUBF2_E_LS_LAST_BYTE_IDX,
        GPS_SUBF2_E_LS_BITS_OF_FIRST_BYTE, GPS_SUBF2_E_LS_BITS_OF_LAST_BYTE);
    eph->eccentricity *= POW_TWO_M33;
    eph->capitalCus =
        POW_TWO_M29 *
        NgExtractTwoCompField(parser->subframe, GPS_SUBF2_CUS_FIRST_BYTE_IDX,
                              GPS_SUBF2_CUS_LAST_BYTE_IDX, GPS_SUBF2_CUS_BITS_OF_FIRST_BYTE,
                              GPS_SUBF2_CUS_BITS_OF_LAST_BYTE, GPS_SUBF2_CUS_FIELD_DIM);
    eph->sqrtA = POW_TWO_P24 * NgExtractField(parser->subframe, GPS_SUBF2_SQRTA_MS_FIRST_BYTE_IDX,
                                              GPS_SUBF2_SQRTA_MS_LAST_BYTE_IDX,
                                              GPS_SUBF2_SQRTA_MS_BITS_OF_FIRST_BYTE,
                                              GPS_SUBF2_SQRTA_MS_BITS_OF_LAST_BYTE);
    eph->sqrtA += NgExtractField(
        parser->subframe, GPS_SUBF2_SQRTA_LS_FIRST_BYTE_IDX, GPS_SUBF2_SQRTA_LS_LAST_BYTE_IDX,
        GPS_SUBF2_SQRTA_LS_BITS_OF_FIRST_BYTE, GPS_SUBF2_SQRTA_LS_BITS_OF_LAST_BYTE);
    eph->sqrtA *= POW_TWO_M19;
    eph->toe = 16.0 * NgExtractField(parser->subframe, GPS_SUBF2_TOE_FIRST_BYTE_IDX,
                                     GPS_SUBF2_TOE_LAST_BYTE_IDX, GPS_SUBF2_TOE_BITS_OF_FIRST_BYTE,
                                     GPS_SUBF2_TOE_BITS_OF_LAST_BYTE);
    if (eph->eccentricity > GPS_MAXIMUM_ECCENTRICITY) {
        return FAILURE_RETURN;
    }
    if (eph->toe > GPS_MAXIMUM_TOC) {
        return FAILURE_RETURN;
    }
    navMsg->subframe2Received = TRUE_FLAG;

    return SUCCESS_RETURN;
}

/** Decode the second orbit segment and record its ephemeris issue.
 * @param parser Channel-local parse context with the validated payload and staging state.
 * @return Parser status. A rejected GPS segment returns a negative value.
 */
static int NgParseGpsSubframe3(TNgNavParser *parser)
{
    TNgBroadcast *navMsg = parser->navMsg;
    TNgOrbit *eph = &(navMsg->ephTmp);
    eph->capitalCic =
        POW_TWO_M29 *
        NgExtractTwoCompField(parser->subframe, GPS_SUBF3_CIC_FIRST_BYTE_IDX,
                              GPS_SUBF3_CIC_LAST_BYTE_IDX, GPS_SUBF3_CIC_BITS_OF_FIRST_BYTE,
                              GPS_SUBF3_CIC_BITS_OF_LAST_BYTE, GPS_SUBF3_CIC_FIELD_DIM);
    eph->capitalOmega0 =
        POW_TWO_P24 * NgExtractTwoCompField(
                          parser->subframe, GPS_SUBF3_OMEGA0_MS_FIRST_BYTE_IDX,
                          GPS_SUBF3_OMEGA0_MS_LAST_BYTE_IDX, GPS_SUBF3_OMEGA0_MS_BITS_OF_FIRST_BYTE,
                          GPS_SUBF3_OMEGA0_MS_BITS_OF_LAST_BYTE, GPS_SUBF3_OMEGA0_MS_FIELD_DIM);
    eph->capitalOmega0 += NgExtractField(
        parser->subframe, GPS_SUBF3_OMEGA0_LS_FIRST_BYTE_IDX, GPS_SUBF3_OMEGA0_LS_LAST_BYTE_IDX,
        GPS_SUBF3_OMEGA0_LS_BITS_OF_FIRST_BYTE, GPS_SUBF3_OMEGA0_LS_BITS_OF_LAST_BYTE);
    eph->capitalOmega0 *= POW_TWO_M31 * NG_BROADCAST_PI;
    eph->capitalCis =
        POW_TWO_M29 *
        NgExtractTwoCompField(parser->subframe, GPS_SUBF3_CIS_FIRST_BYTE_IDX,
                              GPS_SUBF3_CIS_LAST_BYTE_IDX, GPS_SUBF3_CIS_BITS_OF_FIRST_BYTE,
                              GPS_SUBF3_CIS_BITS_OF_LAST_BYTE, GPS_SUBF3_CIS_FIELD_DIM);
    eph->i0 = POW_TWO_P24 * NgExtractTwoCompField(
                                parser->subframe, GPS_SUBF3_I0_MS_FIRST_BYTE_IDX,
                                GPS_SUBF3_I0_MS_LAST_BYTE_IDX, GPS_SUBF3_I0_MS_BITS_OF_FIRST_BYTE,
                                GPS_SUBF3_I0_MS_BITS_OF_LAST_BYTE, GPS_SUBF3_I0_MS_FIELD_DIM);
    eph->i0 += NgExtractField(parser->subframe, GPS_SUBF3_I0_LS_FIRST_BYTE_IDX,
                              GPS_SUBF3_I0_LS_LAST_BYTE_IDX, GPS_SUBF3_I0_LS_BITS_OF_FIRST_BYTE,
                              GPS_SUBF3_I0_LS_BITS_OF_LAST_BYTE);
    eph->i0 *= POW_TWO_M31 * NG_BROADCAST_PI;
    eph->capitalCrc =
        POW_TWO_M05 *
        NgExtractTwoCompField(parser->subframe, GPS_SUBF3_CRC_FIRST_BYTE_IDX,
                              GPS_SUBF3_CRC_LAST_BYTE_IDX, GPS_SUBF3_CRC_BITS_OF_FIRST_BYTE,
                              GPS_SUBF3_CRC_BITS_OF_LAST_BYTE, GPS_SUBF3_CRC_FIELD_DIM);
    eph->omega =
        POW_TWO_P24 * NgExtractTwoCompField(
                          parser->subframe, GPS_SUBF3_OMEGA_MS_FIRST_BYTE_IDX,
                          GPS_SUBF3_OMEGA_MS_LAST_BYTE_IDX, GPS_SUBF3_OMEGA_MS_BITS_OF_FIRST_BYTE,
                          GPS_SUBF3_OMEGA_MS_BITS_OF_LAST_BYTE, GPS_SUBF3_OMEGA_MS_FIELD_DIM);
    eph->omega += NgExtractField(
        parser->subframe, GPS_SUBF3_OMEGA_LS_FIRST_BYTE_IDX, GPS_SUBF3_OMEGA_LS_LAST_BYTE_IDX,
        GPS_SUBF3_OMEGA_LS_BITS_OF_FIRST_BYTE, GPS_SUBF3_OMEGA_LS_BITS_OF_LAST_BYTE);
    eph->omega *= POW_TWO_M31 * NG_BROADCAST_PI;
    eph->capitalOmegaDot =
        POW_TWO_M43 * NG_BROADCAST_PI *
        NgExtractTwoCompField(parser->subframe, GPS_SUBF3_OMEGADOT_FIRST_BYTE_IDX,
                              GPS_SUBF3_OMEGADOT_LAST_BYTE_IDX,
                              GPS_SUBF3_OMEGADOT_BITS_OF_FIRST_BYTE,
                              GPS_SUBF3_OMEGADOT_BITS_OF_LAST_BYTE, GPS_SUBF3_OMEGADOT_FIELD_DIM);
    eph->iode3 = (int)(NgExtractField(
        parser->subframe, GPS_SUBF3_IODE_FIRST_BYTE_IDX, GPS_SUBF3_IODE_LAST_BYTE_IDX,
        GPS_SUBF3_IODE_BITS_OF_FIRST_BYTE, GPS_SUBF3_IODE_BITS_OF_LAST_BYTE));
    eph->iDot =
        POW_TWO_M43 * NG_BROADCAST_PI *
        NgExtractTwoCompField(parser->subframe, GPS_SUBF3_IDOT_FIRST_BYTE_IDX,
                              GPS_SUBF3_IDOT_LAST_BYTE_IDX, GPS_SUBF3_IDOT_BITS_OF_FIRST_BYTE,
                              GPS_SUBF3_IDOT_BITS_OF_LAST_BYTE, GPS_SUBF3_IDOT_FIELD_DIM);
    navMsg->subframe3Received = TRUE_FLAG;

    return SUCCESS_RETURN;
}

/** Recover Galileo system time from word 0 when the time field is available.
 * @param parser Channel-local parse context with the validated payload and staging state.
 * @return Parser status after decoding the supplied I/NAV word.
 */
static int NgParseGalWord00(TNgNavParser *parser)
{
    unsigned char *dataBytes = parser->bitsForParse;
    TNgBroadcast *navMsg = parser->navMsg;
    int time;
    time =
        (int)(NgExtractField(dataBytes, GAL_TYPE0_TIME_FIRST_BYTE_IDX, GAL_TYPE0_TIME_LAST_BYTE_IDX,
                             GAL_TYPE0_TIME_BITS_OF_FIRST_BYTE, GAL_TYPE0_TIME_BITS_OF_LAST_BYTE));
    if (time == 2) {
        /* when the field 'Time' is not set to '10', the fields WN and TOW do not contain valid data
         */
        navMsg->wn =
            (int)(NgExtractField(dataBytes, GAL_TYPE0_WN_FIRST_BYTE_IDX, GAL_TYPE0_WN_LAST_BYTE_IDX,
                                 GAL_TYPE0_WN_BITS_OF_FIRST_BYTE, GAL_TYPE0_WN_BITS_OF_LAST_BYTE));
        navMsg->wn += (int)(parser->wnCorrection);
        navMsg->wnValid = TRUE_FLAG;
        navMsg->tow = (int)(NgExtractField(
            dataBytes, GAL_TYPE0_TOW_FIRST_BYTE_IDX, GAL_TYPE0_TOW_LAST_BYTE_IDX,
            GAL_TYPE0_TOW_BITS_OF_FIRST_BYTE, GAL_TYPE0_TOW_BITS_OF_LAST_BYTE));
        parser->bitIndexTotal = GAL_PAGE_LENGTH;
        navMsg->towValid = TRUE_FLAG;
    }

    return SUCCESS_RETURN;
}

/** Stage Galileo orbit shape and mean anomaly with the navigation issue.
 * @param parser Channel-local parse context with the validated payload and staging state.
 * @return Parser status after decoding the supplied I/NAV word.
 */
static int NgParseGalWord01(TNgNavParser *parser)
{
    unsigned char *dataBytes = parser->bitsForParse;
    TNgBroadcast *navMsg = parser->navMsg;
    TNgOrbit *eph = &(navMsg->ephTmp);
    eph->iode1 =
        (int)(NgExtractField(dataBytes, GAL_TYPE1_IOD_FIRST_BYTE_IDX, GAL_TYPE1_IOD_LAST_BYTE_IDX,
                             GAL_TYPE1_IOD_BITS_OF_FIRST_BYTE, GAL_TYPE1_IOD_BITS_OF_LAST_BYTE));
    eph->toe = GAL_TOE_SCALE_FACTOR *
               NgExtractField(dataBytes, GAL_TYPE1_TOE_FIRST_BYTE_IDX, GAL_TYPE1_TOE_LAST_BYTE_IDX,
                              GAL_TYPE1_TOE_BITS_OF_FIRST_BYTE, GAL_TYPE1_TOE_BITS_OF_LAST_BYTE);
    eph->meanAnomaly0 =
        POW_TWO_M31 * NG_BROADCAST_PI *
        NgExtractTwoCompField(dataBytes, GAL_TYPE1_M0_FIRST_BYTE_IDX, GAL_TYPE1_M0_LAST_BYTE_IDX,
                              GAL_TYPE1_M0_BITS_OF_FIRST_BYTE, GAL_TYPE1_M0_BITS_OF_LAST_BYTE,
                              GAL_TYPE1_M0_BITS_FIELD_DIM);
    eph->eccentricity =
        POW_TWO_M33 * NgExtractField(dataBytes, GAL_TYPE1_E_FIRST_BYTE_IDX,
                                     GAL_TYPE1_E_LAST_BYTE_IDX, GAL_TYPE1_E_BITS_OF_FIRST_BYTE,
                                     GAL_TYPE1_E_BITS_OF_LAST_BYTE);
    eph->sqrtA = POW_TWO_M19 * NgExtractField(dataBytes, GAL_TYPE1_SQRTA_FIRST_BYTE_IDX,
                                              GAL_TYPE1_SQRTA_LAST_BYTE_IDX,
                                              GAL_TYPE1_SQRTA_BITS_OF_FIRST_BYTE,
                                              GAL_TYPE1_SQRTA_BITS_OF_LAST_BYTE);
    navMsg->subframe1Received = TRUE_FLAG;

    return SUCCESS_RETURN;
}

/** Stage Galileo orbit orientation and inclination rate with the navigation issue.
 * @param parser Channel-local parse context with the validated payload and staging state.
 * @return Parser status after decoding the supplied I/NAV word.
 */
static int NgParseGalWord02(TNgNavParser *parser)
{
    unsigned char *dataBytes = parser->bitsForParse;
    TNgBroadcast *navMsg = parser->navMsg;
    TNgOrbit *eph = &(navMsg->ephTmp);
    eph->iode2 =
        (int)(NgExtractField(dataBytes, GAL_TYPE2_IOD_FIRST_BYTE_IDX, GAL_TYPE2_IOD_LAST_BYTE_IDX,
                             GAL_TYPE2_IOD_BITS_OF_FIRST_BYTE, GAL_TYPE2_IOD_BITS_OF_LAST_BYTE));
    eph->capitalOmega0 =
        POW_TWO_M31 * NG_BROADCAST_PI *
        NgExtractTwoCompField(dataBytes, GAL_TYPE2_OMEGA0_FIRST_BYTE_IDX,
                              GAL_TYPE2_OMEGA0_LAST_BYTE_IDX, GAL_TYPE2_OMEGA0_BITS_OF_FIRST_BYTE,
                              GAL_TYPE2_OMEGA0_BITS_OF_LAST_BYTE, GAL_TYPE2_OMEGA0_BITS_FIELD_DIM);
    eph->i0 = POW_TWO_M31 * NG_BROADCAST_PI *
              NgExtractTwoCompField(dataBytes, GAL_TYPE2_I0_FIRST_BYTE_IDX,
                                    GAL_TYPE2_I0_LAST_BYTE_IDX, GAL_TYPE2_I0_BITS_OF_FIRST_BYTE,
                                    GAL_TYPE2_I0_BITS_OF_LAST_BYTE, GAL_TYPE2_I0_BITS_FIELD_DIM);
    eph->omega =
        POW_TWO_M31 * NG_BROADCAST_PI *
        NgExtractTwoCompField(dataBytes, GAL_TYPE2_OMEGA_FIRST_BYTE_IDX,
                              GAL_TYPE2_OMEGA_LAST_BYTE_IDX, GAL_TYPE2_OMEGA_BITS_OF_FIRST_BYTE,
                              GAL_TYPE2_OMEGA_BITS_OF_LAST_BYTE, GAL_TYPE2_OMEGA_BITS_FIELD_DIM);
    eph->iDot =
        POW_TWO_M43 * NG_BROADCAST_PI *
        NgExtractTwoCompField(dataBytes, GAL_TYPE2_IDOT_FIRST_BYTE_IDX,
                              GAL_TYPE2_IDOT_LAST_BYTE_IDX, GAL_TYPE2_IDOT_BITS_OF_FIRST_BYTE,
                              GAL_TYPE2_IDOT_BITS_OF_LAST_BYTE, GAL_TYPE2_IDOT_BITS_FIELD_DIM);
    navMsg->subframe2Received = TRUE_FLAG;

    return SUCCESS_RETURN;
}

/** Stage Galileo secular rates, harmonic corrections and signal accuracy.
 * @param parser Channel-local parse context with the validated payload and staging state.
 * @return Parser status after decoding the supplied I/NAV word.
 */
static int NgParseGalWord03(TNgNavParser *parser)
{
    unsigned char *dataBytes = parser->bitsForParse;
    TNgBroadcast *navMsg = parser->navMsg;
    TNgOrbit *eph = &(navMsg->ephTmp);
    eph->iode3 =
        (int)(NgExtractField(dataBytes, GAL_TYPE3_IOD_FIRST_BYTE_IDX, GAL_TYPE3_IOD_LAST_BYTE_IDX,
                             GAL_TYPE3_IOD_BITS_OF_FIRST_BYTE, GAL_TYPE3_IOD_BITS_OF_LAST_BYTE));
    eph->capitalOmegaDot = POW_TWO_M43 * NG_BROADCAST_PI *
                           NgExtractTwoCompField(dataBytes, GAL_TYPE3_OMEGADOT_FIRST_BYTE_IDX,
                                                 GAL_TYPE3_OMEGADOT_LAST_BYTE_IDX,
                                                 GAL_TYPE3_OMEGADOT_BITS_OF_FIRST_BYTE,
                                                 GAL_TYPE3_OMEGADOT_BITS_OF_LAST_BYTE,
                                                 GAL_TYPE3_OMEGADOT_BITS_FIELD_DIM);
    eph->deltaN =
        POW_TWO_M43 * NG_BROADCAST_PI *
        NgExtractTwoCompField(dataBytes, GAL_TYPE3_DELTAN_FIRST_BYTE_IDX,
                              GAL_TYPE3_DELTAN_LAST_BYTE_IDX, GAL_TYPE3_DELTAN_BITS_OF_FIRST_BYTE,
                              GAL_TYPE3_DELTAN_BITS_OF_LAST_BYTE, GAL_TYPE3_DELTAN_BITS_FIELD_DIM);
    eph->capitalCuc = POW_TWO_M29 * NgExtractTwoCompField(dataBytes, GAL_TYPE3_CUC_FIRST_BYTE_IDX,
                                                          GAL_TYPE3_CUC_LAST_BYTE_IDX,
                                                          GAL_TYPE3_CUC_BITS_OF_FIRST_BYTE,
                                                          GAL_TYPE3_CUC_BITS_OF_LAST_BYTE,
                                                          GAL_TYPE3_CUC_BITS_FIELD_DIM);
    eph->capitalCus = POW_TWO_M29 * NgExtractTwoCompField(dataBytes, GAL_TYPE3_CUS_FIRST_BYTE_IDX,
                                                          GAL_TYPE3_CUS_LAST_BYTE_IDX,
                                                          GAL_TYPE3_CUS_BITS_OF_FIRST_BYTE,
                                                          GAL_TYPE3_CUS_BITS_OF_LAST_BYTE,
                                                          GAL_TYPE3_CUS_BITS_FIELD_DIM);
    eph->capitalCrc = POW_TWO_M05 * NgExtractTwoCompField(dataBytes, GAL_TYPE3_CRC_FIRST_BYTE_IDX,
                                                          GAL_TYPE3_CRC_LAST_BYTE_IDX,
                                                          GAL_TYPE3_CRC_BITS_OF_FIRST_BYTE,
                                                          GAL_TYPE3_CRC_BITS_OF_LAST_BYTE,
                                                          GAL_TYPE3_CRC_BITS_FIELD_DIM);
    eph->capitalCrs = POW_TWO_M05 * NgExtractTwoCompField(dataBytes, GAL_TYPE3_CRS_FIRST_BYTE_IDX,
                                                          GAL_TYPE3_CRS_LAST_BYTE_IDX,
                                                          GAL_TYPE3_CRS_BITS_OF_FIRST_BYTE,
                                                          GAL_TYPE3_CRS_BITS_OF_LAST_BYTE,
                                                          GAL_TYPE3_CRS_BITS_FIELD_DIM);
    eph->sisa = (int)((
        NgExtractField(dataBytes, GAL_TYPE3_SISA_FIRST_BYTE_IDX, GAL_TYPE3_SISA_LAST_BYTE_IDX,
                       GAL_TYPE3_SISA_BITS_OF_FIRST_BYTE, GAL_TYPE3_SISA_BITS_OF_LAST_BYTE)));
    navMsg->subframe3Received = TRUE_FLAG;

    return SUCCESS_RETURN;
}

/** Stage Galileo clock coefficients and the remaining inclination corrections.
 * @param parser Channel-local parse context with the validated payload and staging state.
 * @return Parser status after decoding the supplied I/NAV word.
 */
static int NgParseGalWord04(TNgNavParser *parser)
{
    unsigned char *dataBytes = parser->bitsForParse;
    TNgBroadcast *navMsg = parser->navMsg;
    int svId;
    TNgOrbit *eph = &(navMsg->ephTmp);
    TNgBroadcastClock *clk = &(navMsg->clkTmp);
    eph->iode4 =
        (int)(NgExtractField(dataBytes, GAL_TYPE4_IOD_FIRST_BYTE_IDX, GAL_TYPE4_IOD_LAST_BYTE_IDX,
                             GAL_TYPE4_IOD_BITS_OF_FIRST_BYTE, GAL_TYPE4_IOD_BITS_OF_LAST_BYTE));
    svId =
        (int)(NgExtractField(dataBytes, GAL_TYPE4_PRN_FIRST_BYTE_IDX, GAL_TYPE4_PRN_LAST_BYTE_IDX,
                             GAL_TYPE4_PRN_BITS_OF_FIRST_BYTE, GAL_TYPE4_PRN_BITS_OF_LAST_BYTE));
    navMsg->satelliteId = svId;
    eph->capitalCic = POW_TWO_M29 * NgExtractTwoCompField(dataBytes, GAL_TYPE4_CIC_FIRST_BYTE_IDX,
                                                          GAL_TYPE4_CIC_LAST_BYTE_IDX,
                                                          GAL_TYPE4_CIC_BITS_OF_FIRST_BYTE,
                                                          GAL_TYPE4_CIC_BITS_OF_LAST_BYTE,
                                                          GAL_TYPE4_CIC_BITS_FIELD_DIM);
    eph->capitalCis = POW_TWO_M29 * NgExtractTwoCompField(dataBytes, GAL_TYPE4_CIS_FIRST_BYTE_IDX,
                                                          GAL_TYPE4_CIS_LAST_BYTE_IDX,
                                                          GAL_TYPE4_CIS_BITS_OF_FIRST_BYTE,
                                                          GAL_TYPE4_CIS_BITS_OF_LAST_BYTE,
                                                          GAL_TYPE4_CIS_BITS_FIELD_DIM);
    clk->toc =
        60 * NgExtractField(dataBytes, GAL_TYPE4_COC_FIRST_BYTE_IDX, GAL_TYPE4_COC_LAST_BYTE_IDX,
                            GAL_TYPE4_COC_BITS_OF_FIRST_BYTE, GAL_TYPE4_COC_BITS_OF_LAST_BYTE);
    clk->af[0] = POW_TWO_M34 * NgExtractTwoCompField(
                                   dataBytes, GAL_TYPE4_AF0_FIRST_BYTE_IDX,
                                   GAL_TYPE4_AF0_LAST_BYTE_IDX, GAL_TYPE4_AF0_BITS_OF_FIRST_BYTE,
                                   GAL_TYPE4_AF0_BITS_OF_LAST_BYTE, GAL_TYPE4_AF0_BITS_FIELD_DIM);
    clk->af[1] = POW_TWO_M46 * NgExtractTwoCompField(
                                   dataBytes, GAL_TYPE4_AF1_FIRST_BYTE_IDX,
                                   GAL_TYPE4_AF1_LAST_BYTE_IDX, GAL_TYPE4_AF1_BITS_OF_FIRST_BYTE,
                                   GAL_TYPE4_AF1_BITS_OF_LAST_BYTE, GAL_TYPE4_AF1_BITS_FIELD_DIM);
    clk->af[2] = POW_TWO_M59 * NgExtractTwoCompField(
                                   dataBytes, GAL_TYPE4_AF2_FIRST_BYTE_IDX,
                                   GAL_TYPE4_AF2_LAST_BYTE_IDX, GAL_TYPE4_AF2_BITS_OF_FIRST_BYTE,
                                   GAL_TYPE4_AF2_BITS_OF_LAST_BYTE, GAL_TYPE4_AF2_BITS_FIELD_DIM);
    navMsg->subframe4Received = TRUE_FLAG;

    return SUCCESS_RETURN;
}

/** Decode Galileo ionosphere, broadcast group delays, time and E1-B health.
 * @param parser Channel-local parse context with the validated payload and staging state.
 * @return Parser status after decoding the supplied I/NAV word.
 */
static int NgParseGalWord05(TNgNavParser *parser)
{
    unsigned char *dataBytes = parser->bitsForParse;
    TNgBroadcast *navMsg = parser->navMsg;
    TNgOrbit *eph = &(navMsg->ephTmp);

    TNgBroadcastClock *clk = &(navMsg->clkTmp);
    int flag;
    navMsg->iono.alphaGal[0] =
        POW_TWO_M02 * NgExtractField(dataBytes, GAL_TYPE5_A0I_FIRST_BYTE_IDX,
                                     GAL_TYPE5_A0I_LAST_BYTE_IDX, GAL_TYPE5_A0I_BITS_OF_FIRST_BYTE,
                                     GAL_TYPE5_A0I_BITS_OF_LAST_BYTE);
    navMsg->iono.alphaGal[1] =
        POW_TWO_M08 *
        NgExtractTwoCompField(dataBytes, GAL_TYPE5_A1I_FIRST_BYTE_IDX, GAL_TYPE5_A1I_LAST_BYTE_IDX,
                              GAL_TYPE5_A1I_BITS_OF_FIRST_BYTE, GAL_TYPE5_A1I_BITS_OF_LAST_BYTE,
                              GAL_TYPE5_A1I_BITS_FIELD_DIM);
    navMsg->iono.alphaGal[2] =
        POW_TWO_M15 *
        NgExtractTwoCompField(dataBytes, GAL_TYPE5_A2I_FIRST_BYTE_IDX, GAL_TYPE5_A2I_LAST_BYTE_IDX,
                              GAL_TYPE5_A2I_BITS_OF_FIRST_BYTE, GAL_TYPE5_A2I_BITS_OF_LAST_BYTE,
                              GAL_TYPE5_A2I_BITS_FIELD_DIM);
    flag = (int)NgExtractField(dataBytes, GAL_TYPE5_SF1_FIRST_BYTE_IDX, GAL_TYPE5_SF1_LAST_BYTE_IDX,
                               GAL_TYPE5_SF1_BITS_OF_FIRST_BYTE, GAL_TYPE5_SF1_BITS_OF_LAST_BYTE);
    navMsg->iono.sf[0] = (flag > 0) ? TRUE_FLAG : FALSE_FLAG;
    flag = (int)NgExtractField(dataBytes, GAL_TYPE5_SF2_FIRST_BYTE_IDX, GAL_TYPE5_SF2_LAST_BYTE_IDX,
                               GAL_TYPE5_SF2_BITS_OF_FIRST_BYTE, GAL_TYPE5_SF2_BITS_OF_LAST_BYTE);
    navMsg->iono.sf[1] = (flag > 0) ? TRUE_FLAG : FALSE_FLAG;
    flag = (int)NgExtractField(dataBytes, GAL_TYPE5_SF3_FIRST_BYTE_IDX, GAL_TYPE5_SF3_LAST_BYTE_IDX,
                               GAL_TYPE5_SF3_BITS_OF_FIRST_BYTE, GAL_TYPE5_SF3_BITS_OF_LAST_BYTE);
    navMsg->iono.sf[2] = (flag > 0) ? TRUE_FLAG : FALSE_FLAG;
    flag = (int)NgExtractField(dataBytes, GAL_TYPE5_SF4_FIRST_BYTE_IDX, GAL_TYPE5_SF4_LAST_BYTE_IDX,
                               GAL_TYPE5_SF4_BITS_OF_FIRST_BYTE, GAL_TYPE5_SF4_BITS_OF_LAST_BYTE);
    navMsg->iono.sf[3] = (flag > 0) ? TRUE_FLAG : FALSE_FLAG;
    flag = (int)NgExtractField(dataBytes, GAL_TYPE5_SF5_FIRST_BYTE_IDX, GAL_TYPE5_SF5_LAST_BYTE_IDX,
                               GAL_TYPE5_SF5_BITS_OF_FIRST_BYTE, GAL_TYPE5_SF5_BITS_OF_LAST_BYTE);
    navMsg->iono.sf[4] = (flag > 0) ? TRUE_FLAG : FALSE_FLAG;
    clk->bgd_E1E5a =
        POW_TWO_M32 * NgExtractTwoCompField(dataBytes, GAL_TYPE5_TGD_E1E5A_FIRST_BYTE_IDX,
                                            GAL_TYPE5_TGD_E1E5A_LAST_BYTE_IDX,
                                            GAL_TYPE5_TGD_E1E5A_BITS_OF_FIRST_BYTE,
                                            GAL_TYPE5_TGD_E1E5A_BITS_OF_LAST_BYTE,
                                            GAL_TYPE5_TGD_E1E5A_BITS_FIELD_DIM);
    clk->bgd_E1E5b =
        POW_TWO_M32 * NgExtractTwoCompField(dataBytes, GAL_TYPE5_TGD_E1E5B_FIRST_BYTE_IDX,
                                            GAL_TYPE5_TGD_E1E5B_LAST_BYTE_IDX,
                                            GAL_TYPE5_TGD_E1E5B_BITS_OF_FIRST_BYTE,
                                            GAL_TYPE5_TGD_E1E5B_BITS_OF_LAST_BYTE,
                                            GAL_TYPE5_TGD_E1E5B_BITS_FIELD_DIM);
    clk->tGroupDelay = clk->bgd_E1E5b;
    navMsg->wn =
        (int)(NgExtractField(dataBytes, GAL_TYPE5_WN_FIRST_BYTE_IDX, GAL_TYPE5_WN_LAST_BYTE_IDX,
                             GAL_TYPE5_WN_BITS_OF_FIRST_BYTE, GAL_TYPE5_WN_BITS_OF_LAST_BYTE));
    navMsg->wn += (int)(parser->wnCorrection);
    navMsg->wnValid = TRUE_FLAG;
    navMsg->tow =
        (int)(NgExtractField(dataBytes, GAL_TYPE5_TOW_FIRST_BYTE_IDX, GAL_TYPE5_TOW_LAST_BYTE_IDX,
                             GAL_TYPE5_TOW_BITS_OF_FIRST_BYTE, GAL_TYPE5_TOW_BITS_OF_LAST_BYTE));
    navMsg->towValid = TRUE_FLAG;
    parser->bitIndexTotal = GAL_PAGE_LENGTH;
    eph->health = (int)(NgExtractField(
        dataBytes, GAL_TYPE5_E1BHS_FIRST_BYTE_IDX, GAL_TYPE5_E1BHS_LAST_BYTE_IDX,
        GAL_TYPE5_E1BHS_BITS_OF_FIRST_BYTE, GAL_TYPE5_E1BHS_BITS_OF_LAST_BYTE));
    eph->galileoDataValidity = (int)(NgExtractField(
        dataBytes, GAL_TYPE5_E1BDVS_FIRST_BYTE_IDX, GAL_TYPE5_E1BDVS_LAST_BYTE_IDX,
        GAL_TYPE5_E1BDVS_BITS_OF_FIRST_BYTE, GAL_TYPE5_E1BDVS_BITS_OF_LAST_BYTE));
    navMsg->subframe5Received = TRUE_FLAG;
    navMsg->ionoReceived = TRUE_FLAG;

    return SUCCESS_RETURN;
}

/** Publish a coherent orbit and clock pair after basic physical range checks.
 * GPS matches the low eight bits of IODC against IODE in subframes 2 and 3.
 * Galileo matches the issue across words 1 through 4 and waits for word 5.
 * Until the staged values pass these checks, the previously published pair is kept.
 * @param broadcast Channel-local cache containing the staged and published issues.
 * @param isGalileo Nonzero for Galileo I/NAV, zero for GPS LNAV.
 */
static void Commit(TNgBroadcast *broadcast, int isGalileo)
{
    int issueComplete = broadcast->subframe1Received && broadcast->subframe2Received &&
                        broadcast->subframe3Received;
    if (isGalileo) {
        issueComplete = issueComplete && broadcast->subframe4Received &&
                        broadcast->subframe5Received &&
                        broadcast->ephTmp.iode1 == broadcast->ephTmp.iode2 &&
                        broadcast->ephTmp.iode2 == broadcast->ephTmp.iode4;
    } else {
        issueComplete =
            issueComplete && ((broadcast->clkTmp.iodc & 255) == broadcast->ephTmp.iode2);
    }
    issueComplete = issueComplete && broadcast->ephTmp.iode2 == broadcast->ephTmp.iode3;
    if (issueComplete && isfinite(broadcast->ephTmp.sqrtA) && broadcast->ephTmp.sqrtA > 4000 &&
        broadcast->ephTmp.sqrtA < 7000 && broadcast->ephTmp.eccentricity >= 0 &&
        broadcast->ephTmp.eccentricity < 1 && broadcast->ephTmp.toe >= 0 &&
        broadcast->ephTmp.toe < 604800 && broadcast->clkTmp.toc >= 0 &&
        broadcast->clkTmp.toc < 604800) {
        broadcast->eph = broadcast->ephTmp;
        broadcast->clk = broadcast->clkTmp;
        broadcast->ephemeridesReceived = 1;
    }
}

/** Decode the GPS broadcast ionosphere model from LNAV subframe 4, page 18.
 * Interpret each of the eight coefficients as a signed octet, then apply its
 * prescribed power-of-two scale. Alpha coefficients precede beta coefficients.
 * @param parser Channel-local parse context containing the validated GPS subframe.
 */
static void NgParseGpsIono(TNgNavParser *parser)
{
    static const int coefficientStartBytes[8] = {9, 10, 12, 13, 14, 16, 17, 18};
    static const int coefficientExponents[8] = {-30, -27, -24, -24, 11, 14, 16, 16};
    TNgBroadcast *broadcast = parser->navMsg;
    unsigned page = (unsigned)NgExtractField(parser->subframe, 8, 9, 4, 2);
    unsigned dataId = (unsigned)NgExtractField(parser->subframe, 8, 8, 4, 6);
    unsigned coefficientIndex;
    if (dataId != 1 || page != 56) {
        return;
    }
    for (coefficientIndex = 0; coefficientIndex < 8; ++coefficientIndex) {
        double coefficientValue = ldexp(
            (double)NgExtractTwoCompField(parser->subframe, coefficientStartBytes[coefficientIndex],
                                          coefficientStartBytes[coefficientIndex] + 1, 6, 2, 8),
            coefficientExponents[coefficientIndex]);
        if (coefficientIndex < 4) {
            broadcast->iono.alphaGps[coefficientIndex] = coefficientValue;
        } else {
            broadcast->iono.betaGps[coefficientIndex - 4] = coefficientValue;
        }
    }
    broadcast->ionoReceived = 1;
}

/** Retain Classic GPS field positions, including word parity gaps. */
int NgBroadcastGps(TNgBroadcast *broadcast, const uint32_t words[10], unsigned id)
{
    TNgNavParser parser = {0};
    int result = 0;
    unsigned wordIndex;
    parser.navMsg = broadcast;
    for (wordIndex = 0; wordIndex < 10; wordIndex++) {
        parser.subframe[4 * wordIndex] = (unsigned char)(words[wordIndex] >> 24);
        parser.subframe[4 * wordIndex + 1] = (unsigned char)(words[wordIndex] >> 16);
        parser.subframe[4 * wordIndex + 2] = (unsigned char)(words[wordIndex] >> 8);
        parser.subframe[4 * wordIndex + 3] = (unsigned char)words[wordIndex];
    }
    if (id == 1) {
        result = NgParseGpsSubframe1(&parser);
    } else if (id == 2) {
        result = NgParseGpsSubframe2(&parser);
    } else if (id == 3) {
        result = NgParseGpsSubframe3(&parser);
    } else if (id == 4) {
        NgParseGpsIono(&parser);
    }
    if (!result) {
        Commit(broadcast, 0);
    }
    return result;
}

/** Retain Classic Galileo word fields; optional authentication and RS are separate modules. */
int NgBroadcastGalileo(TNgBroadcast *broadcast, const unsigned char data[16])
{
    TNgNavParser parser = {0};
    unsigned wordType = data[0] >> 2;
    parser.navMsg = broadcast;
    memcpy(parser.bitsForParse, data, 16);
    switch (wordType) {
    case 0:
        NgParseGalWord00(&parser);
        break;
    case 1:
        NgParseGalWord01(&parser);
        break;
    case 2:
        NgParseGalWord02(&parser);
        break;
    case 3:
        NgParseGalWord03(&parser);
        break;
    case 4:
        NgParseGalWord04(&parser);
        break;
    case 5:
        NgParseGalWord05(&parser);
        break;
    case 6: {
        /* I/NAV Table 47: the signed 8-bit GST-UTC leap term follows A0 and A1. */
        const int encodedLeapSeconds = ((data[7] & 3) << 6) | (data[8] >> 2);
        broadcast->iono.utcLeap = (encodedLeapSeconds ^ 128) - 128;
        break;
    }
    default:
        break;
    }
    Commit(broadcast, 1);
    return broadcast->towValid && (wordType == 5 || (wordType == 0 && (data[0] & 3u) == 2));
}
