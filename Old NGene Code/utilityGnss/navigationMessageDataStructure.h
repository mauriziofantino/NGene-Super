/**
 * @file navigationMessageDataStructure.h
 *
 * @author Mario Nicola
 *
 * @date 03/10/2014
 *
 * @version 1.0.0
 *
 * @brief This header defines constants and types related to the
 *        navigation message of the supported signals.
 */

#ifndef NAVIGATION_MESSAGE_DATA_STRUCTURE_H_INCLUDED
#define NAVIGATION_MESSAGE_DATA_STRUCTURE_H_INCLUDED

/**
 * @enum DataValidityStatusEnum
 *
 * @brief This is the type that indicates the validity of the transmitted navigation message.
 *        Used for Galileo only.
 */
typedef enum DataValidityStatusEnum
{
	NAVIGATION_DATA_VALID=0,	/**< Value in the Galileo SIS ICD */
	WORKING_WITHOUT_GUARANTEE=1	/**< Value in the Galileo SIS ICD */
} TDataValidityStatus;

/**
 * @enum SignalHealthStatusEnum
 *
 * @brief This is the type that indicates the validity of the transmitted navigation message.
 * @details For Galileo all the possible values in the SIS ICD are reported;
 *          for GPS, only the most significant bit is considered.
 */
typedef enum SignalHealthStatusEnum
{
	SIGNAL_OK=0,                          	/**< GPS, most significant bit of the satellite health: All signals OK*/
	SIGNAL_OUT_OF_SERVICE=1,                /**< GPS, most significant bit of the satellite health: unhealthy */
	SIGNAL_WILL_BE_OUT_OF_SERVICE=2,        /**< not used for GPS */
	SIGNAL_COMPONENT_CURRENTLY_UNDER_TEST=3 /**< not used for GPS */
} TSignalHealthStatus;

/**
 * @def SISA_NOT_AVAILABLE
 * @brief Value of SISA (Galileo navigation message) when the accuracy prediction is not available
 */
#define SISA_NOT_AVAILABLE	255

/**
 * @def URA_NOT_AVAILABLE
 * @brief Value of User Range Accuracy (GPS navigation message) when the accuracy prediction is not available
 */
#define URA_NOT_AVAILABLE	15

/**
 * @struct GnssEphemeridesStruct
 * @brief Contains the ephemerides
 */
typedef struct GnssEphemeridesStruct
{
	/* Common fields */
	/* Issue Of Data Ephemerides */
	int iode1;					/**< IODE read in word 1 of the Galileo navigation message */
	int iode2;					/**< IODE read in subframe 2/word 2 of the GPS/Galileo navigation message */
	int iode3;					/**< IODE read in subframe 3/word 3 of the GPS/Galileo navigation message */
	int iode4;					/**< IODE read in word 4 of the Galileo navigation message */
	/* Ephemerides */
	double meanAnomaly0;		/**< Mean anomaly @ toe */
	double deltaN;				/**< Mean motion difference */
	double eccentricity;		/**< Eccentricity */
	double sqrtA;				/**< Square root of the semi-major axis */
	double capitalOmega0;		/**< Longitude of ascending node @ toe */
	double i0;					/**< Inclination angle */
	double omega;				/**< Argument of perigee */
	double capitalOmegaDot;		/**< Rate of right ascension */
	double iDot;				/**< Rate of inclination angle */
	double capitalCuc;			/**< Amplitude of the cosine harmonic correction term to the argument of latitude */
	double capitalCus;			/**< Amplitude of the sine harmonic correction term to the argument of latitude */
	double capitalCrc;			/**< Amplitude of the cosine harmonic correction term to the orbit radius */
	double capitalCrs;			/**< Amplitude of the sine harmonic correction term to the orbit radius */
	double capitalCic;			/**< Amplitude of the cosine harmonic correction term to the angle of inclination */
	double capitalCis;			/**< Amplitude of the sine harmonic correction term to the angle of inclination */
	double toe;					/**< Reference time (Ephemerides) */
	/* Signal goodness */
	TSignalHealthStatus health;	/**< SV health */
	int sisa;					/**< Signal-In-Space Accuracy (for GPS: URA, Use Range Accuracy) */
	/* Galileo dedicated fields */
	TDataValidityStatus galileoDataValidity; 	/**< Validity status of the Galileo navigation data */
} TGnssEphemerides;

/**
 * @struct GnssClkStruct
 * @brief Contains the parameters used to correct the satellite clock
 */
typedef struct GnssClkStruct
{
	double toc;			/**< Reference time (Clock) */
	double af[3];		/**< Satellite clock correction coefficients */
	double tGroupDelay;	/**< Group delay */
	double bgd_E1E5a;	/**< Broadcast Group delay E1,E5a (Galileo) */
	double bgd_E1E5b;	/**< Broadcast Group delay E1,E5b (Galileo) */
	int iodc;			/**< Issue Of Data */
} TGnssClk;

/**
 * @struct GnssRedCedStruct
 * @brief Contains the Reduced Clock and Ephemeris Data (Red CED).
 *        Used only for Galileo I/NAV v2.0
 */
typedef struct GnssRedCedStruct
{
	int tor;           /**< Reduced CED reference time [s] */
	double deltaARed;  /**< Semi-major axis (difference w.r.t. the nominal value) */
	double exRed;      /**< RedCED eccentricity vector, x component */
	double eyRed;      /**< RedCED eccentricity vector, y component */
	double deltaI0Red; /**< Inclination angle (difference w.r.t. the nominal value */
	double omega0Red;  /**< Longitude of ascending node  */
	double lambda0Red; /**< Mean argument of latitude */
	double af0Red;     /**< Satellite clock bias correction coefficient */
	double af1Red;     /**< Satellite clock drift correction coefficient */
} TGnssRedCed;

/**
 * @struct GnssIonoStruct
 * @brief Contains the data used to get the ionospheric corrections
 */
typedef struct GnssIonoStruct
{
    /* Galileo dedicated fields */
    double alphaGal[3];		/**< Effective ionisation level parameters (NeQuick) */
    int sf[5];				/**< Storm Flag (Ionospheric disturbance flag) */
    /* GPS dedicated fields */
    double alphaGps[4];		/**< Ionisation Level parameters (Klobuchar model) */
    double beta[4];			/**< Ionisation Level parameters (Klobuchar model) */
} TGnssIono;

/**
 * @struct GnssToUtcTimeConversionStruct
 * @brief Contains the parameters used to convert from sat to UTC time
 */
typedef struct GnssToUtcTimeConversionStruct
{
    double a0;	/**< Constant term of polynomial */
    double a1;	/**< 1st order term of polynomial */
    int dtLs;	/**< Leap second cunt before leap second adjustment */
    int t0T;	/**< UTC data reference Time Of Week */
    int wn0T;	/**< UTC data reference Week Number */
    int wnLsf;	/**< Week Number of leap second adjustment */
    int dn;		/**< Day Number at the end of which a leap second adjustment becomes effective */
    int dtLsf;	/**< Leap second count after leap second adjustment */
} TGnssToUtcTimeConversion;

/**
 * @struct GalileoToGpsTimeConversionStruct
 * @brief Contains the parameters used to convert from the Galileo time to the GPS time
 */
typedef struct GalileoToGpsTimeConversionStruct
{
    double a0G;		/**< Constant term of the time offset */
    double a1G;		/**< Rate of change of the time offset */
    int t0G;		/**< GGTO (GPS to Galileo Time Offset) data reference Time Of Week */
    int wn0G;	/**< GGTO (GPS to Galileo Time Offset) data reference Week Number */
} TGalileoToGpsTimeConversion;

/**
 * @struct GnssAlmanacStruct
 * @brief Contains the almanac received from other satellites
 */
typedef struct GnssAlmanacStruct
{
	int ioda;					/**< Almanac issue of data */
	double toa;					/**< Almanac reference time */
	int wna;					/**< Almanac reference week number */
	double sqrtA;				/**< Square root of the semi-major axis (Galileo: delta w.r.t. the nominal value) */
	double eccentricity;		/**< Eccentricity */
	double i0;					/**< Inclination angle (Galileo: delta w.r.t. the nominal value) */
	double capitalOmega0;		/**< Longitude of ascending node @ toe */
	double capitalOmegaDot;		/**< Rate of right ascension */
	double omega;				/**< Argument of perigee */
	double meanAnomaly0;		/**< Mean anomaly @ toe */
	double af[2];				/**< Satellite clock correction coefficients */
	TSignalHealthStatus health;	/**< SV health */
} TGnssAlmanac;


/**
 * @struct NavigationMessageStruct
 * @brief Contains the results of the navigation message parse,
 *        for each possible PRN.
 */
typedef struct NavigationMessageStruct
{
	int tow; 							/**< Time Of Week [s] */
	int towValid;						/**< TRUE_FLAG when the current TOW is valid */
	int wn;								/**< Week Number */
	int wnValid;						/**< TRUE_FLAG when the current WN is valid */
	/* Ephemerides and Clock corrections */
	TGnssEphemerides ephTmp;			/**<  */
	TGnssEphemerides eph;				/**<  */
	TGnssClk clkTmp;					/**<  */
	TGnssClk clk;						/**<  */
	int subframe1Received;				/**< If TRUE_FLAG, subframe 1/word 1 has been received for the current satellite */
	int subframe2Received;				/**< If TRUE_FLAG, subframe 2/word 2 has been received for the current satellite */
	int subframe3Received;				/**< If TRUE_FLAG, subframe 3/word 3 has been received for the current satellite */
	int subframe4Received;				/**< If TRUE_FLAG, word 4 has been received for the current satellite (Galileo Only) */
	int subframe5Received; 				/**< If TRUE_FLAG, word 5 has been received for the current satellite (Galileo Only) */
	int ephemeridesReceived; 			/**< If TRUE_FLAG, ephemerides and clock corrections are ready for the use in PVT */
	/* Red CED (Galileo only) */
	TGnssRedCed redCed;                 /**<  */
	int redCedReceived;                 /**<  */
	int ephFromRedCed;                  /**< If TRUE_FLAG, current ephemerides and clock corrections are derived from Red CED */
	/* Ionospheric corrections */
	TGnssIono iono;						/**< Ionospheric corrections */
	int ionoReceived;					/**< If TRUE_FLAG, ionospheric corrections have been received */
	/* Time conversions */
	TGnssToUtcTimeConversion gnss2Utc;	/**< Parameters to pass from GNSS time to UTC time */
	int gnss2UtcReceived;				/**< If TRUE_FLAG, GNSS time to UTC time parameters have been received */
	int gnss2UtcCompleted;              /**< if TRUE_FLAG, the correct value of wn0T has been computed (broadcast wn0T is truncated) */
	TGalileoToGpsTimeConversion	gal2Gps;/**< Parameters to pass from Galileo time to GPS time */
	int gal2GpsReceived; 				/**< If TRUE_FLAG, Galileo time to GPS time parameters have been received */
	int gal2GpsCompleted;               /**< if TRUE_FLAG, the correct value of wn0G has been computed (broadcast wn0G is truncated) */
	/* Almanac */
	TGnssAlmanac almanac;				/**< */
	int almanac1Received;				/**< For GPS: if TRUE_FLAG, the almanac for the current satellite has been received
	 	 	 	 	 	 	 	 	 	 	 For Galileo: if TRUE_FLAG, the 1st part of the almanac for the current satellite has been received */
	int almanac2Received;				/**< If TRUE_FLAG, the 2nd part of the almanac for the current satellite has been received (Galileo only) */
} TNavigationMessage;


/* TODO Verificare se i fattori di conversione (GNSS-UTC, Galileo-GPS) sono uguali per tutti i PRN:
 *      Nel caso spostare fuori dal canale, tenerne una copia unica
 **/


#endif /* NAVIGATION_MESSAGE_DATA_STRUCTURE_H_INCLUDED */
