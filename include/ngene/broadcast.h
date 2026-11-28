/** @file broadcast.h @brief Classic broadcast orbit/clock values; double SI units.
 * Adapted from navigationMessageDataStructure.h, Mario Nicola, 2014. */
#ifndef NGENE_BROADCAST_H
#define NGENE_BROADCAST_H

typedef struct TNgOrbit {
    /* Common fields */
    /* Issue Of Data Ephemerides */
    int iode1; /**< IODE read in word 1 of the Galileo navigation message */
    int iode2; /**< IODE read in subframe 2/word 2 of the GPS/Galileo navigation message */
    int iode3; /**< IODE read in subframe 3/word 3 of the GPS/Galileo navigation message */
    int iode4; /**< IODE read in word 4 of the Galileo navigation message */
    /* Ephemerides */
    double meanAnomaly0;    /**< Mean anomaly @ toe */
    double deltaN;          /**< Mean motion difference */
    double eccentricity;    /**< Eccentricity */
    double sqrtA;           /**< Square root of the semi-major axis */
    double capitalOmega0;   /**< Longitude of ascending node @ toe */
    double i0;              /**< Inclination angle */
    double omega;           /**< Argument of perigee */
    double capitalOmegaDot; /**< Rate of right ascension */
    double iDot;            /**< Rate of inclination angle */
    double capitalCuc;      /**< Amplitude of the cosine harmonic correction term to the argument of
                               latitude */
    double capitalCus;      /**< Amplitude of the sine harmonic correction term to the argument of
                               latitude */
    double capitalCrc; /**< Amplitude of the cosine harmonic correction term to the orbit radius */
    double capitalCrs; /**< Amplitude of the sine harmonic correction term to the orbit radius */
    double capitalCic; /**< Amplitude of the cosine harmonic correction term to the angle of
                          inclination */
    double capitalCis; /**< Amplitude of the sine harmonic correction term to the angle of
                          inclination */
    double toe;        /**< Reference time (Ephemerides) */
    /* Signal goodness */
    int health; /**< SV health */
    int sisa;   /**< Signal-In-Space Accuracy (for GPS: URA, Use Range Accuracy) */
    /* Galileo dedicated fields */
    int galileoDataValidity; /**< Validity status of the Galileo navigation data */
    double axisRate;         /**< B1C rate of semi-major axis [m/s]. */
    double meanMotionRate;   /**< B1C rate of mean-motion correction [rad/s²]. */
} TNgOrbit;

typedef struct TNgBroadcastClock {
    double toc;           /**< Reference time (Clock) */
    double af[3];         /**< Satellite clock correction coefficients */
    double tGroupDelay;   /**< Group delay */
    double bgd_E1E5a;     /**< Broadcast Group delay E1,E5a (Galileo) */
    double bgd_E1E5b;     /**< Broadcast Group delay E1,E5b (Galileo) */
    int iodc;             /**< Issue Of Data */
    double beidouDataIsc; /**< B1C data-to-pilot signal correction [s]. */
    double beidouB2aTgd;  /**< B2a pilot group delay [s], stored from B-CNAV1. */
} TNgBroadcastClock;

/** Consistent issue-of-data cache, owned by one channel. */
typedef struct {
    TNgOrbit ephTmp, eph;          /**< Pending and committed orbit. */
    TNgBroadcastClock clkTmp, clk; /**< Pending and committed clock. */

    struct {
        double alphaGal[3]; /**< Galileo I/NAV word 5 effective ionization coefficients. */
        double alphaBds[9]; /**< B-CNAV1 SF3 page 1 BDGIM coefficients (TECU). */
        int utcLeap;        /**< Broadcast whole-second system time offset for ionosphere models. */
        unsigned bdsTow;    /**< BDT seconds of week of the accepted BDGIM page. */
        double alphaGps[4], betaGps[4]; /**< LNAV SF4 page 18 Klobuchar coefficients. */
        int sf[5];
    } iono; /**< Broadcast ionosphere model for this channel. */

    int satelliteId;                /**< Broadcast SVID when available; zero before reception. */
    int wn, tow, wnValid, towValid; /**< Broadcast system time, unexpanded week. */
    int subframe1Received, subframe2Received, subframe3Received, subframe4Received,
        subframe5Received;                 /**< Parts received. */
    int ionoReceived, ephemeridesReceived; /**< Cache validity. */
} TNgBroadcast;
#endif
