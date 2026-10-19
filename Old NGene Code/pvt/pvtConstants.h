/**
 * @file pvtConstants.h
 *
 * @author Mario Nicola
 *
 * @date 23/10/2014
 *
 * @version 1.0.0
 *
 * @brief Some constants used during the PVT computation.
 */

#ifndef PVT_CONSTANTS_H_INCLUDED
#define PVT_CONSTANTS_H_INCLUDED


// LA DEFINE ENABLE_PVT_BIN_FILE NON E' UTILIZZATA
// MA VIENE MANTENUTA QUI PER DOCUMENTARE IL FORMATO DEL LOG
/**
 * @def ENABLE_PVT_BIN_FILE
 * @brief If this symbol is defined a binary log file will be created for the PVT computation
 * @details Fields saved in the binary file are (for each execution of the PVT)
 *
 *			unsigned int	pvtIndex
 *			double			UTC TOW (-1.0 if not valid)
 *			double			GPS TOW (-1.0 if not valid)
 *			int				GPS WN (-1.0 if not valid)
 *			double			Galileo TOW (-1.0 if not valid)
 *			int 			Galileo WN (-1.0 if not valid)
 *			double X5 		GDOP, PDOP, HDOP, VDOP, TDOP
 *			double X3		ECEF rx pos [m]
 *			double  		rx longitude [deg]
 *			double 			rx latitude [deg]
 *			double 			rx altitude [m]
 *			double X3 		ECEF rx vel [m/s]
 *			int 			GPS bias validity (1=>valid, 0=>not valid)
 *			double 			GPS bias [m]
 *			int 			Galileo bias validity (1=>valid, 0=>not valid)
 *			double 			Galileo bias [m]
 *			double 			drift [m/s]
 *			double		    Time tag expressed as number of samples from the beginning of the file [s]
 *			int				Number of channels ready for PVT
 *			int				Number of GPS channels ready for PVT
 *			int				Number of Galileo channels ready for PVT
 *			int				Number of channels used in PVT
 *			int 			Egnos Flag
 *
 *			for each channel
 *				int 		satType (1=>GPS, 2=>Galileo)
 *				int 		satId
 *				double		Ttx [s]
 *				int 		WN
 *				double  	fd [Hz]
 *				double 		pseudorange raw [m]
 *				double 		clock correction [m]
 *				double 		tropo correction [m]
 *				double 		iono correction [m]
 *				double 		carrier smoothing measure [rad]
 *				double 		pseudorange corrected and smoothed [m]
 *				double X3	ECEF sat pos [m]
 *				double X3	ECEF sat vel [m/s]
 *				double 		sat azimuth [rad]
 *				double 		sat elevation [rad]
 *				double      accumulated Doppler (Rinex-line) [cycles]
 *				char	    egnos iono correction flag
 *				double      egnos pseudorange fast correction [m]
 *				double    	egnos long term clock correction [s]
 *				double    	egnos long term ECEF x position correction [m]
 *				double    	egnos long term ECEF y position correction [m]
 *				double    	egnos long term ECEF z position correction [m]
 *			end
 *
 */
//#define ENABLE_PVT_BIN_FILE



#define K1 						(77.604)
#define RD 						(287.054)
#define P0_45 					(1.015750000000000e+03)
#define GM1 					(9.7840)
#define LAMBDA0_45 				(2.570000000000000)
#define BETA0_45 				(0.005580000000000)
#define E0_45 					(11.660000000000000)
#define T0_45 					(2.831500000000000e+02)
#define K2						(382000)
#define GRAVITY_ACCELERATION  	(9.80665)

// WGS84 ellipsoid constants
#define A_CONST					(6378137.0)            // Earth's semi-major axis (in m) (WGS-84)
#define B_CONST					(6356752.3142)
#define E_CONST					(8.1819190842622e-2)
#define E_SQUARE_CONST          (E_CONST*E_CONST)

/**
 * @def GM
 * @brief Earth's universal gravitational parameter [m^3/s^2]
 */
#define GM		(3.986004418000000e+14)

#define F_CONST	(-4.442807309000000e-10)

/**
 * @def OMEGAE_DOT
 * @brief Earth rotation rate, [rad/s]
 */
#define OMEGAE_DOT  (7.2921151467e-5)
#define WEDOT		(7.292115147000000e-5)

/**
 * @def SPEED_OF_LIGHT
 * @brief Speed of light [m/s]
 */
#define SPEED_OF_LIGHT 			(2.99792458e+8)

/**
 * @def DEFAULT_PSEUDORANGE_GPS
 * @brief Default value for the GPS reference pseudorange [s]
 */
#define DEFAULT_PSEUDORANGE_GPS   (70.0e-3)

/**
 * @def DEFAULT_PSEUDORANGE_GAL
 * @brief Default value for the Galileo reference pseudorange [s]
 */
#define DEFAULT_PSEUDORANGE_GAL   (78.0e-3)

/**
 * @def GPS_MAXIMUM_TTX_DIFFERENCE
 * @brief The maximum difference between two GPS satellites (to be verified)
 */
#define GPS_MAXIMUM_TTX_DIFFERENCE	30e-3

/**
 * @def GALILEO_MAXIMUM_TTX_DIFFERENCE
 * @brief The maximum difference between two Galileo satellites (to be verified)
 */
#define GALILEO_MAXIMUM_TTX_DIFFERENCE	40e-3

/**
 * @def MINIMUM_NUMBER_OF_CHANNELS_FOR_PVT
 * @brief The minimum number of operative channel in order to be able to execute PVT
 * 		  when LS is performed
 */
#define MINIMUM_NUMBER_OF_CHANNELS_FOR_PVT 4

/**
 * @def NUMBER_OF_LS_BASED_PVTS
 * @brief If required by the user, the PVT passes to the KF algorithm
 * 		  after NUMBER_OF_LS_BASED_PVTS LS-based PVTs.
 */
#define NUMBER_OF_LS_BASED_PVTS 2

/**
 * @def LS_MULTI_SIZE
 * @brief Number of positions unknowns when Least Squares is executed
 *        on a multiple constellations case (5=>2 constellations, 6=>3 constellations,...)
 */
#define LS_MULTI_SIZE	5

/**
 * @def LS_SINGLE_SIZE
 * @brief Number of positions unknowns when Least Squares is executed
 *        on a single constellation case
 */
#define LS_SINGLE_SIZE	4

/**
 * @def LS_ITERATIONS
 * @brief Number of iterations in LeastSquares method
 */
#define LS_ITERATIONS	(7)


/**
 * @def KF_MULTI_SIZE
 * @brief Number of positions unknowns when Kalman Filter is executed
 *        on a multiple constellations case (5=>2 constellations, 6=>3 constellations,...)
 */
#define KF_MULTI_SIZE	5

/**
 * @def KF_SINGLE_SIZE
 * @brief Number of positions unknowns when Kalman Filter is executed
 *        on a single constellation case
 */
#define KF_SINGLE_SIZE	4

/**
 * @def KF_MULTI_STATES
 * @brief Number of Kalman states when Kalman Filter is executed
 *        on a multiple constellations case
 */
#define KF_MULTI_STATES	9

/**
 * @def KF_SINGLE_STATES
 * @brief Number of Kalman states when Kalman Filter is executed
 *        on a single constellation case
 */
#define KF_SINGLE_STATES	8

#define LAMBDA 		(0.1902936728)
#define KF_SP		(5e-2)
#define KF_SF		(1e-1)
#define KF_SG		(1e-2)
#define KF_POS_SIGMA2_LOW_VALUE    (1e1)
#define KF_POS_SIGMA2_HIGH_VALUE   (1e3)
#define KF_VEL_SIGMA2          	   (1e0)


#endif /* PVT_CONSTANTS_H_INCLUDED */
