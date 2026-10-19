/**
 * @file spreadingCodes.h
 *
 * @author Mario Nicola
 *
 * @date 08/08/2014
 *
 * @version 1.0.0
 *
 * @brief This header defines main constants of the GPS,GPS and Galileo signals.
 *        It declares also the function that loads from file all the spreading codes.
 */
#ifndef SPREADING_CODES_H_INCLUDED
#define SPREADING_CODES_H_INCLUDED


/**
 * @def L1_FREQUENCY
 * @brief central frequency for GPS L1/Galileo E1 signals
 */
#ifndef L1_FREQUENCY
#define L1_FREQUENCY (1575420000.0)
#endif


/**
 * @def INVALID_PRN
 *
 * @brief An invalid PRN is indicated with index -1.
 *
 * @details Each PRN is indicated with an index.
 *         For GPS C/A signal, PRNs are indicated with number from 1 to 32.
 */
#ifndef INVALID_PRN
#define INVALID_PRN (-1)
#endif

/**
 * @def BASE_PRN
 *
 * @brief The first index for both GPS and Galileo PRNs is 1.
 */
#define BASE_PRN 				1

/**
 * @def GPS_NUMBER_OF_PRNS
 * @brief Total number of spreading code for GPS L1/CA signal
 */
#define GPS_NUMBER_OF_PRNS		32
/**
 * @def GPS_CODE_RATE
 * @brief Code rate [chip/s] for GPS L1/CA signal
 */
#define GPS_CODE_RATE   		1023000.0
/**
 * @def GPS_CODE_LEN
 * @brief Length of a code period [chip] for GPS L1/CA signal
 */
#define GPS_CODE_LEN			1023
/**
 * @def GPS_CODE_DURATION
 * @brief Nominal duration of a code period [ms] for GPS L1/CA signal
 */
#define GPS_CODE_DURATION 		1
/**
 * @def GPS_BIT_DURATION
 * @brief Nominal duration of a bit [ms] for GPS L1/CA signal
 */
#define GPS_BIT_DURATION 		20

/**
 * @def GALILEO_NUMBER_OF_PRNS
 * @brief Total number of spreading code for Galileo E1bc signal
 */
#define GALILEO_NUMBER_OF_PRNS	50
/**
 * @def GALILEO_CODE_RATE
 * @brief Code rate [chip/s] for Galileo E1bc signal
 */
#define GALILEO_CODE_RATE   	1023000.0
/**
 * @def GALILEO_CODE_LEN
 * @brief Length of a code period [chip] for Galileo E1bc signal
 */
#define GALILEO_CODE_LEN		4092
/**
 * @def GALILEO_CODE_DURATION
 * @brief Nominal duration of a code period [ms] for Galileo E1bc signal
 */
#define GALILEO_CODE_DURATION 	4
/**
 * @def GALILEO_BIT_DURATION
 * @brief Nominal duration of a bit [ms] for Galileo E1bc signal
 */
#define GALILEO_BIT_DURATION	4
/**
 * @def GALILEO_E5A_BIT_DURATION
 * @brief Nominal duration of a bit [ms] for Galileo E5a signal
 */
#define GALILEO_E5A_BIT_DURATION	20


/**
 * @def EGNOS_NUMBER_OF_PRNS
 * @brief Total number of spreading code for EGNOS signal
 */
#define EGNOS_NUMBER_OF_PRNS	4

/**
 * @def EGNOS_FIRST_PRN
 * @brief First supported EGNOS PRN
 */
#define EGNOS_FIRST_PRN        120

/**
 * @def EGNOS_SECOND_PRN
 * @brief Second supported EGNOS PRN
 */
#define EGNOS_SECOND_PRN       124

/**
 * @def EGNOS_THIRD_PRN
 * @brief Third supported EGNOS PRN
 */
#define EGNOS_THIRD_PRN        126

/**
 * @def EGNOS_FOURTH_PRN
 * @brief Fourth supported EGNOS PRN
 */
#define EGNOS_FOURTH_PRN       136

/**
 * @def EGNOS_CODE_RATE
 * @brief Code rate [chip/s] for EGNOS signal
 */
#define EGNOS_CODE_RATE   		1023000.0
/**
 * @def EGNOS_CODE_LEN
 * @brief Length of a code period [chip] for EGNOS signal
 */
#define EGNOS_CODE_LEN			1023
/**
 * @def EGNOS_CODE_DURATION
 * @brief Nominal duration of a code period [ms] for EGNOS signal
 */
#define EGNOS_CODE_DURATION 	1
/**
 * @def EGNOS_BIT_DURATION
 * @brief Nominal duration of a bit [ms] for EGNOS signal
 */
#define EGNOS_BIT_DURATION 		2

/**
 * @enum SatConstellationEnum
 *
 * @brief is the type that indicates the constellation of the current PRN.
 */
typedef enum SatConstellationEnum
{
	INVALID_SAT=0, /**< Default no-good value */
    GPS_SAT=1,     /**< GPS PRN */
    GALILEO_SAT=2, /**< Galileo PRN */
    EGNOS_SAT=3    /**< EGNOS PRN */
} TSatConstellation;

/**
 *  @enum SatSignalEnum
 *  @brief combined with SatConstellationEnum, indicates the band and the signal of the current PRN
 */
typedef enum SatSignalEnum
{
	INVALID_SAT_SIGNAL, /**< invalid default value */
	L1_CA,              /**< GPS L1 C/A */
	L1_C,               /**< GPS L1 C, for CHIMERA */
	E1_BC,              /**< Galileo OS on E1 */
	E5_A,               /**< Galileo E5a (F/NAV) */
	E5_B,               /**< Galileo E5b (I/NAV) */
	L1                  /**< EGNOS */
} TSatSignal;



#endif /* SPREADING_CODES_H_INCLUDED */
