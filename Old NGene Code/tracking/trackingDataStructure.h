/**
 * @file trackingDataStructure.h
 *
 * @author Mario Nicola
 *
 * @date 03/09/2014
 *
 * @version 1.0.0
 *
 * @brief Data structures used to store tracking variables.
 */

#ifndef TRACKING_DATA_STRUCTURE_H_INCLUDED
#define TRACKING_DATA_STRUCTURE_H_INCLUDED

#include "cno.h"

/**
 * @def FLL_TIMEOUT
 * @brief FLL should be locked before this timeout [ms]
 */
#define FLL_TIMEOUT		4000

/**
 * @def PLL_TIMEOUT
 * @brief PLL should be locked before this timeout [ms]
 */
#define PLL_TIMEOUT 	8000

/**
 * @def GPS_NAVIGATION_MESSAGE_TIMEOUT
 * @brief The synchronization of the GPS navigation message must be found before this timeout [ms]
 */
#define GPS_NAVIGATION_MESSAGE_TIMEOUT	18000

/**
 * @def GAL_NAVIGATION_MESSAGE_TIMEOUT
 * @brief The synchronization of the Galileo navigation message must be found before this timeout [ms]
 */
#define GAL_NAVIGATION_MESSAGE_TIMEOUT	8000

/**
 * @def RELOCK_TIMEOUT
 * @brief After a lock error occurs, tracking must be locked again before this timeout [ms]
 */
#define RELOCK_TIMEOUT 	1000


/**
 * @def PLL_CHECKER_SUMS
 * @brief Metrics used to verify PLL lock are averaged over PLL_CHECKER_SUMS ms
 */
#define PLL_CHECKER_SUMS	100

/**
 * @def CNO_CHECK_PERIOD
 * @brief Number of ms between two consecutive checks of the estimated CNo
 */
#define CNO_CHECK_PERIOD 	100

/**
 * @def CNO_HIGH_THRESHOLD_DIFF
 * @brief Difference between the CN0 cutoff (from locked to under investigation) and the CN0 high threshold (from under investigation to locked) [dB]
 */
#define CNO_HIGH_THRESHOLD_DIFF 	2

/**
 * @def PLL_CHECKER_LOW_THRESHOLD
 * @brief The estimator of the PLL lock is compared with this threshold in order
 *        to find a PLL lock failure
 */
#define PLL_CHECKER_LOW_THRESHOLD 0.4

/**
 * @def PLL_CHECKER_HIGH_THRESHOLD
 * @brief The estimator of the PLL lock is compared with this threshold in order
 *        to find the PLL lock success
 */
#define PLL_CHECKER_HIGH_THRESHOLD 0.8

/**
 * @def PLL_FALSE_LOCK_CHECKER_PERIOD
 * @brief The check of the PLL false lock is repeated every PLL_FALSE_LOCK_CHECKER_PERIOD ms.
 */
#define PLL_FALSE_LOCK_CHECKER_PERIOD 	10000

/**
 * @def PLL_FALSE_LOCK_CHECKER_TIMEOUT
 * @brief The check of the PLL false lock is executed PLL_FALSE_LOCK_CHECKER_TIMEOUT ms
 *        before a PLL timeout.
 */
#define PLL_FALSE_LOCK_CHECKER_TIMEOUT 	500

/**
 * @def PLL_FALSE_LOCK_FILTER_LENGTH
 *
 * @brief Length of the Hatch filter used to clean the code rate estimation
 *        used as input of the experimental PLL false lock detection and correction
 */
#define PLL_FALSE_LOCK_FILTER_LENGTH     200


/**
 * @enum TrackingStatusEnum
 * @brief Used to represent the current status of the tracking
 */
typedef enum TrackingStatusEnum
{
	TRACKING_INITIAL_TRANSIENT,		/**< Tracking just started, not locked yet */
	TRACKING_PLL_FALSE_LOCK_CHECK,  /**< Status used to manage the check of the PLL */
	TRACKING_PLL_FALSE_LOCK_CONFIRMATION,/**< Status used to confirm the correction of the PLL false lock */
	TRACKING_LOCKED,				/**< Tracking is locked to the input signal */
	TRACKING_PLL_CHECK_FAILED,      /**< An error from the PLL check has been found */
	TRACKING_CN0_CHECK_FAILED,      /**< An error from the CN0 check has been found */
	TRACKING_LOST 					/**< Tracking is not able to lock the input signal, tracking ceases */
} TTrackingStatus;


/**
 * @typedef  CarrierLoopChoiceEnum
 *
 * @brief    Is used when the current active filter in carrier tracking is represented.
 * It's returned by ChooseCarrierLoop and GIOVEA_ChooseCarrierLoop functions.
 */
typedef enum CarrierLoopChoiceEnum
{
    CARRIER_FLL, 				/**< It's used when tracking performs FLL and DLL */
    CARRIER_PLL  				/**< It's used when tracking performs PLL and DLL */
} TCarrierLoopChoice;

/**
 * @brief This structure stores the variables used to manage carrier loops (FLL and PLL)
 */
typedef struct FllPllStruct
{
	double fllT2T1;		/**< Coefficient for FLL */
	double fllDeltaT1;	/**< Coefficient for FLL */
	double pllT2T1;		/**< Coefficient for PLL */
	double pllDeltaT1;	/**< Coefficient for PLL */
	double delta;		/**< Integration time [s] */
	double initialCarrierFrequency;	/**< Initial estimation of carrier frequency [Hz]. FLL and PLL correct this */
	double errort;		/**< Output of the discriminator */
	double errortOld;	/**< Output of the discriminator (previous integration) */
	double carrierNcoOld;		/**< Output of the loop filter (previous integration) */
	/**/
	TAverageWindow meanCarrierFreq;	/**< Average the estimated carrier frequency (used to pass from FLL to PLL) */
	double meanCarrierFreqOld;		/**< Mean carrier frequency (previous estimation) */
} TFllPll;

/**
 * @brief This structure stores the variables used to manage code loop (DLL)
 */
typedef struct DllStruct
{
	double dllT2T1;				/**< Coefficient for DLL */
	double dllDeltaT1;			/**< Coefficient for DLL */
	double initialCodeRate;		/**< Initial estimation of code rate [Hz]. DLL corrects this */
	double dllDiscriminatorFactor; 	/**< Factor used to multiply the DLL discriminator (depends on the tracked signal) */
	double dllDiscriminator;	/**< Output of the discriminator */
	double dllDiscriminatorOld;	/**< Output of the discriminator (previous integration) */
	double codeNcoOld;			/**< Output of the loop filter (previous integration) */
} TDll;


/**
 * @brief Values representing the status of the PLL False Lock Checker
 */
typedef enum TrackingPflcStatusEnum
{
	PFLC_RUNNING,
	PFLC_FINISHED
} TTrackingPflcStatus;

/**
 * @brief Structure used to manage the status of the tracking
 * @details Includes the CNo estimation.
 */
typedef struct TrackingStatusCheckerStruct
{
	/* cno estimator */
	TCnoTable cnoEstimator;					/**< CNo estimator */
	/* pll lock estimator */
	unsigned long long int pllStartCounter;	/**< trkCounter value when the PLL replaces the FLL */
	TAverageWindow pIMean;					/**< mean value of prompt in-phase correlation with 1ms integration period */
	TAverageWindow pQMean;					/**< mean value of prompt quadrature correlation with 1ms integration period */
	double nbd;								/**< first metric to be used in the detector */
	double nbp;								/**< second metric to be used in the detector */
	/* pll false lock checker */
	int pflcArraySize;						/**< length of arrays used in pflc */
	double *pflcLocks;						/**< array that stores the fd of possible false locks  */
	double *pflcDistances;					/**< array that stores the distances of the estimated fd and the false lock fds */
	int *pflcTests;							/**< how many tests have been passed for each possible locks */
	int pflcTotalTests;						/**< how many tests have been performed */
	/* variables for the several checks */
	unsigned long long int pflcLastCheckCounter;    /**< value of trkCounter when the last check on the PLL false lock has been performed */
	unsigned long long int pllLastCheckCounter;	 	/**< value of trkCounter when the last check on the PLL lock has been performed */
	unsigned long long int trkCounterForPllError;	/**< value of trkCounter when the last error in PLL lock has been found */
	unsigned long long int cnoLastCheckCounter;		/**< value of trkCounter when the last check on the CNo has been performed */
	unsigned long long int trkCounterForCnoError;	/**< value of trkCounter when the last error in CNo has been found */
} TTrackingStatusChecker;


/**
 * @struct TrackingStruct
 *
 * @brief Contains the tracking variables.
 */
typedef struct TrackingStruct
{
	TTrackingStatus trkStatus; 			/**< Represents the current state of the tracking */
	int msCounter;						/**< Counter of the ms in the current bit */
	unsigned long long int trkCounter;	/**< Counts execution ms from the start of the tracking */
	unsigned long long int lockCounter;	/**< Stores the value of trkCounter when the tracking is considered locked*/
	int integrationPeriod;				/**< Current length of a correlation [ms] */
	int integrationPeriodUser;			/**< Length of the correlation chosen by the user [ms] */
	float spacing;  					/**< Early-Prompt Spacing (== Late-Prompt) for this channel [chip] */
	/* other status variables */
	int fllLocked;						/**< TRUE_FLAG when the FLL is estimated locked */
	int bitSynchronizationFound;		/**< TRUE_FLAG when the bit synchronization is found */
	int integrationPeriodUserActive;	/**< TRUE_FLAG when tracking uses the integration period chosen by the user */
	/* code phase (early/late for low fs only) */
	double codeEarlyPhase;			/**< Code phase used to generate the early replica of the despreading code at this execution cycle [chip] */
	double codeEarlyPhaseFrac;		/**< Fractional part of codePhase early [chip] */
	double codeLatePhase;			/**< Code phase used to generate the early replica of the despreading code at this execution cycle [chip] */
	double codeLatePhaseFrac;		/**< Fractional part of codePhase early [chip] */
	/* code pointers */
	char *earlyCode;	/**< Points to the early code */
	char *promptCode;	/**< Points to the prompt code */
	char *lateCode;		/**< Points to the late code */
	char **multiCodes;	/**< Array with all the pointers to the code used for multiple correlators */
	char *sidePeakLeftCode;	    /**< Additional correlator to check if the the Galileo signal is locked to the left side peak after the Fast Re-acquisition */
	char *sidePeakRightCode;	/**< Additional correlator to check if the the Galileo signal is locked to the right side peak after the Fast Re-acquisition */
	float sideLeftI;		/**< side left peak in-phase correlation */
	float sideLeftQ;		/**< side left peak quadrature correlation */
	float sideRightI;		/**< side right peak in-phase correlation */
	float sideRightQ;		/**< side right peak quadrature correlation */
	double sideLeft;		/**< side left peak correlation (modulus)*/
	double sideRight;		/**< side right peak correlation (modulus)*/
	double p;		        /**< prompt correlation (modulus)*/
	/* correlations */
	float eI;		/**< early in-phase correlation */
	float eQ;		/**< early quadrature correlation */
	float pI;		/**< prompt in-phase correlation */
	float pQ;		/**< prompt quadrature correlation */
	float lI;		/**< late in-phase correlation */
	float lQ;		/**< late quadrature correlation */
	#ifdef CSC_DISCRIMINATOR_FUNCTION
	char *veryEarlyCode;	/**< Points to the very early code */
	char *veryLateCode;		/**< Points to the very late code */
	float vEI;		/**< very early in-phase correlation */
	float vEQ;		/**< very early quadrature correlation */
	float vLI;		/**< very late in-phase correlation */
	float vLQ;		/**< very late quadrature correlation */
	#endif
	float pIOld;	/**< prompt in-phase correlation (previous integration) */
	float pQOld;	/**< prompt quadrature correlation (previous integration) */
	float pIForNav;	/**< prompt in-phase correlation (used for navigation message decoding). Integration period always equal to bit duration. */
	float pIForCno;	/**< prompt in-phase correlation (used for CNo estimation). Integration period always equal to 1 ms. */
	float pQForCno;	/**< prompt quadrature correlation (used for CNo estimation). Integration period always equal to 1 ms. */
	float eOld;		/**< early correlation (modulus) from the previous integration */
	float pOld;		/**< prompt correlation (modulus) from the previous integration */
	float lOld;		/**< late correlation (modulus) from the previous integration */
	/* multiple correlations */
	int numberOfMulti; 	/**< number of multiple correlations */
	float *multiI;		/**< multiple correlations in phase */
	float *multiQ;		/**< multiple correlations quadrature */
	float *multiOld;	/**< multiple correlations (modulus) from the previous integration */
	/* discriminators and loops */
	TFllPll carrierLoop;		/**< discriminator and filter used to estimate carrier frequency (and phase) */
	TDll codeLoop;				/**< discriminator and filter used to estimate code rate */
	/* GPS and EGNOS bit synchronization */
	int bitSynchronizationIndex;	/**< number of valid sign transitions (multiple of the bit duration) */
	unsigned long long int lastValidDataTransition;	/**< trkCounter when the last good sign transition occurred */
	/* check tracking status */
	TTrackingStatusChecker trkStatusChecker;	/**< this structure stores variables used to verify the tracking lock */
} TTracking;


#endif /* TRACKING_DATA_STRUCTURE_H_INCLUDED */
