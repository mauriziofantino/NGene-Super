/**
 * @file ngeneConfiguration.h
 *
 * @author Mario Nicola
 *
 * @date 06/08/2014
 *
 * @version 1.0.0
 *
 * @brief This header defines the structure that stores NGene configuration
 *        from both GUI and configuration files.
 */

#ifndef NGENE_CONFIGURATION_H_INCLUDED
#define NGENE_CONFIGURATION_H_INCLUDED

#include <stdio.h>
#include <pthread.h>
#include "ngeneConstants.h"
#include "pvtConfiguration.h"
#include "acqConfiguration.h"
#include "trkConfiguration.h"
#include "warmStartConfiguration.h"
#include "usbFrontEndConfiguration.h"
#include "spreadingCodes.h"
#include "prnListDataStructure.h"
#include "ncursesDataStructure.h"
#include "rinexDataStructure.h"
#include "navigationMessageConfiguration.h"

/**
 * @enum UserInterfaceEnum
 *
 * @brief Enumeration used to represent the chosen user interface.
 */
typedef enum UserInterfaceEnum
{
	RX_UI_TEXT,		/**< Symbol used in configuration structure to indicate that the user interface is textual */
	RX_UI_GUI,	  	/**< Symbol used in configuration structure to indicate that the user interface is graphical */
	RX_UI_NCURSES	/**< Symbol used in configuration structure to indicate that the user interface is ncurses based */
} TUserInterface;

/**
 * @def USB_FRONTEND
 * @brief This define is used to indicate whenever the input samples are read from a USB front-end
 */
#define USB_FRONTEND  0
/**
 * @def INPUT_FILE
 * @brief This define is used to indicate whenever the input samples are read from a binary file
 */
#define INPUT_FILE    1

/**
 * @struct ManualChannelConfigurationStruct
 *
 * @brief Contains all the configuration parameters sent by client for manually configured channel
 */
typedef struct ManualChannelConfigurationStruct
{
	TSatConstellation satType;	/**< Constellation of the satellite assigned to this manually configured channel */
	int satId;					/**< Index of the PRN of the satellite assigned to this manually configured channel */
    float dllBw;				/**< DLL bandwidth (in Hz) */
    float pllBw;				/**< PLL bandwidth (in Hz) */
    float coherentTrkIntTime;	/**< Coherent track integration time (in ms) */
    float coherentAcqIntTime;	/**< Coherent acquisition integration time (in ms). It's unused in the current release. */
    float nonCoherentAcqIntTime;/**< Number of non-coherent sums used in acquisition. */
    float spacing;				/**< Early-Late DLL spacing (in chips) */
	#ifdef CSC_DISCRIMINATOR_FUNCTION
    float veryEarlyVeryLateSpacing;				/**< Very Early-Very Late DLL spacing (in chips) */
	#endif
} TManualChannelConfiguration;

/**
 * @struct GuiConfigurationStruct
 *
 * @brief Contains all the configuration parameters sent by client (not present in text configuration files)
 */
typedef struct GuiConfigurationStruct
{
	/* Channels configuration */
	int automaticConfigurationFlag; 	/**< If TRUE_FLAG, then NGene autonomously allocates PRNs to channels,
	                                     	 if FALSE_FLAG, then user allocates PRNs to channels through GUI. */

	float gpsCoherentAcqIntTime;		/**< Coherent acquisition integration time for GPS (in ms). It's unused in the current release. */
	float galileoCoherentTrkIntTime;	/**< Coherent track integration time for Galileo (in ms). Fixed value equal to 4 ms */
	float galileoCoherentAcqIntTime;	/**< Coherent acquisition integration time for Galileo (in ms). It's unused in the current release. */
	float egnosCoherentAcqIntTime;		/**< Coherent acquisition integration time for EGNOS (in ms). It's unused in the current release. */
	int numberOfExcludedPrns;			/**< User can exclude some PRNs from elaboration through the GUI */
	TPrnList excludedPrns;              /**< List of excluded PRNs */
	TManualChannelConfiguration *manuallyConfiguredChannels; /**< list of channels manually configured */
	/* Spectrum, histogram, logs */
	int spectrumSamples;				/**< number of samples per spectrum */
	int spectrumWindows;				/**< number of windows per spectrum */
	int spectrumOverlap;				/**< overlap for spectrum */
	int spectrumZeroPadding;			/**< zero padding for spectrum */
	int spectrumWeightFunction;			/**< weight function for spectrum */
	int histSamples;					/**< number of samples used to compute histogram */
	int histBits;						/**< number of bits for the computation of the histogram */
	int outputPathLength;				/**< Output path string length; if 0 the following field is not present */
	char *outputPath;					/**< path of the client output folder (see interface document page 8) */
	int ifGenerationFlag;				/**< generation of IF samples file */
	/* multiple correlators */
	int multipleCorrelatorsFlag;		/**< multiple correlators are used */
	int numberOfMultipleCorrelators;	/**< number of multiple correlators to be used */
	int multipleCorrelatorsSpacing;		/**< spacing between multiple correlators [samples] */
} TGuiConfiguration;

#define RINEX_STATION_NAME_LEN    4
#define RINEX_AGENCY_NAME_LEN    20
#define RINEX_MARKER_NAME_LEN    60
#define RINEX_MARKER_TYPE_LEN    20
#define RINEX_OBSERVER_LEN       20
#define RINEX_AGENCY_LEN         40
#define RINEX_REC_NUM_LEN        20
#define RINEX_ANT_NUM_LEN        20
#define RINEX_ANT_TYPE_LEN       20
#define NMEA_TALKER_ID_LEN        2
#define NMEA_PROPRIETARY_ID_LEN   4

/**
 * @struct NGeneLogConfigurationStruct
 *
 * @brief Contains all the configuration parameters related to the log performed by NGene
 */
 typedef struct NGeneLogConfigurationStruct
{
	int enableAcqLog;
	int enableTrkLog;
	int enableNavMesgLog;
	int enableNavMesgIfenLog;
	int enableAlmanacLog;
	int enableOsnmaLog;
	int enableOsnmaSimpleLog;
	int enablePvtTextLog;
	int enablePvtBinLog;
	int enableGgtoLog;
	// RINEX
	int enableRinexNav;
	int enableRinexObs;
	char rinexStation[RINEX_STATION_NAME_LEN+1]; /**< The station name is used for the name of the RINEX file */
	char rinexAgency[RINEX_AGENCY_NAME_LEN+1];
	char rinexMarkerName[RINEX_MARKER_NAME_LEN+1];
	char rinexMarkerType[RINEX_MARKER_TYPE_LEN+1];
	char rinexObserver[RINEX_OBSERVER_LEN+1];
	char rinexAgency2[RINEX_AGENCY_LEN+1];
	char rinexReceiverNumber[RINEX_REC_NUM_LEN+1];
	char rinexAntennaNumber[RINEX_ANT_NUM_LEN+1];
	char rinexAntennaType[RINEX_ANT_TYPE_LEN+1];
	float rinexAntennaDelta[3];
	// NMEA
	int enableNmea;
	int nmeaAutomaticFilename;
	char nmeaFilename[MAX_ROW_LENGTH];
	char nmeaTalkerId[NMEA_PROPRIETARY_ID_LEN+1];
} TNGeneLogConfiguration;

/**
 * @struct NGeneConfigurationStruct
 *
 * @brief Contains the NGene receiver configuration.
 *
 * @details All parameters are read from configuration text files.
 */
typedef struct NGeneConfigurationStruct
{
	int configurationValid; 					/**< if FALSE_FLAG, the de-initialization function has already been executed */
	/* command line parameters */
	const char *progname;
	char logFilename[MAX_ROW_LENGTH];			/**< Logging file name plus path */
	char googleEarthFilename[MAX_ROW_LENGTH];	/**< Google Earth output file */
	int googleEarthFilenameValid; 				/**< If TRUE_FLAG, the googleEarthFilename is valid and the KML file will be created */
	unsigned int verbose;						/**< Verbose flag */
	TUserInterface userInterface;				/**< Receiver user interface */
	unsigned int guiComPort; 					/**< Port used to communicate with GUI through a TCP connection */
	FILE *stdlog;		      					/**< External Logging FILE descriptor */
	pthread_mutex_t stdLogMutex;				/**< Used to grant mutual exclusion when multiple threads are used */
	/* current paths */
	int sudoer;                                 /**< TRUE_FLAG if the receiver has been launched with sudo, else FALSE_FLAG */
	char homePath[MAX_ROW_LENGTH];				/**< Path for the home folder of the current user */
	char configurationPath[MAX_ROW_LENGTH];		/**< Path for configuration files */
	char otherDataPath[MAX_ROW_LENGTH];			/**< Path for several data files */
	char dumpPath[MAX_ROW_LENGTH/2];            /**< Initial path where the NGene logs are saved */
	char dumpPathCurrent[MAX_ROW_LENGTH];		/**< Current path where the NGene logs are saved
	                                                 (can differ from dumpPath when logs are split) */
	/* dump path configuration */
	int splitLogsEnabled;                       /**< If TRUE_FLAG the receiver logs are split */
	int splitLogsPeriod; 						/**< Maximum duration (hours) of a log when split */
	int splitLogsAligned;                       /**< if TRUE_FLAG, the logs are split aligned to the hour */
	/* GNSS_RX.conf */
	int usedFrontEnd;							/**< Index of the selected front-end */
	int inputMethod;                            /**< Binary file or USB front-end */
	char *sampleFilename;		                /**< Path of the input samples file */
	int numberOfGpsChannels;					/**< Number of GPS channels */
	int numberOfGalileoChannels;				/**< Number of Galileo channels */
	int numberOfEgnosChannels;					/**< Number of EGNOS channels */
	int numberOfGalileoE5aChannels;             /**< Number of Galileo channels for E5a (F/NAV) */
	int numberOfChannels;                       /**< Total number of supported channels */
	int receiverClockEmulation;					/**< TRUE_FLAG if PC date/time are used to emulate the receiver clock */
	int receiverClockError;                     /**< Absolute value of the maximum error of the receiver clock
	                                                 (valid if receiverClockEmulation is TRUE_FLAG) */
	int allGalileoPrnsEnabled;					/**< if TRUE_FLAG, all GAlileo PRNs are elaborated, else just PRNs of real satellites */
	int automaticConfigurationFlag; 			/**< If TRUE_FLAG, then NGene autonomously allocates PRNs to channels,
		                                     	     if FALSE_FLAG, then user allocates PRNs to channels through a configuration file. */
	TManualChannelConfiguration *manuallyConfiguredChannels; /**< list of channels manually configured */
	/* PVT */
	TPvtConfiguration pvtConfiguration;			/**< PVT configuration */
	/* Acquisition */
	TAcqConfiguration acqConfiguration;  		/**< Acquisition configuration */
	/* Tracking */
	TTrkConfiguration trkConfiguration;			/**< Tracking configuration */
	/* Warm Start */
	TWarmStartConfiguration warmConfiguration;  /**< Warm start configuration */
	/* FE parameters */
	TUsbFeConfiguration usbFeParameters;		/**< List of all supported USB front-ends and their parameters */
	/* Navigation message */
	TNavMsgConfiguration navMsgConfiguration;   /**< Parse of the Navigation Message */
	/* GUI */
	TGuiConfiguration guiParameters;			/**< All parameters (exclusively) from the GUI */
	/* Logs */
	TNGeneLogConfiguration logConfiguration; /**< Configuration of the receiver logs */
	/* Additional information for GUI log */
	char *msgToGui;
	/* Additional information for NCurses log */
	int ncursesDataInitialized;
	TNCursesData *ncursesData;
	/* Variable whose values derive from the previous parameter */
	TUsbFePars *fePars;	/**< Points to the parameters of the selected front-end */
	double fs;			/**< Sampling frequency [Hz] */
	double ts;           /**< Sampling period [s] */
	double fif;			/**< Current intermediate frequency [Hz] */
	double samplesPerMillisecond; /**< Number of samples in one millisecond */
	double samplesPerChip; /**< Number of samples in one chip (all signals have same code rate!) */
	unsigned int pvtPeriodMilliseconds; /**< Number of ms between two consecutive PVT computations */
	double pvtPeriodSeconds;      /**< Number of s between two consecutive PVT computations */
	int lowFsElaboration;      /**< TRUE_FLAG if the fs is low and requires an ad hoc elaboration, FALSE_FLAG otherwise */
	int disableGpsElaboration; /**< If TRUE_FLAG the GPS and EGNOS signals will not be elaborated due to a low fs */
	int disableGalElaboration; /**< If TRUE_FLAG the Galileo signal will not be elaborated due to a low fs */
} TNGeneConfiguration;



/* Configurations that cannot be chosen by user */

/**
 * @def FFT_OF_LOCAL_CODES_FROM_FILE
 * @brief If defined, FFT of local codes (used for acquisition) are read from a file
 *        If not defined, FFT of local codes are computed during receiver initialization.
 */
#define FFT_OF_LOCAL_CODES_FROM_FILE

#ifndef FFT_OF_LOCAL_CODES_FROM_FILE

	/**
	 * @def SAVE_FFT_OF_LOCAL_CODES
	 * @brief If the FFT of local codes are computed at the receiver initialization and this
	 *        symbol is defined, FFT of local codes are saved into a file for future use.
	 */
	#define SAVE_FFT_OF_LOCAL_CODES
#endif


/**
 * @def DISABLE_CONFIRMATION_THRESHOLD
 * @brief If defined, the threshold of the confirmation is derived by scaling the threshold
 *        of the coarse acquisition. Otherwise it is evaluated by a custom procedure.
 *        By default this symbol is defined because the custom procedure returns
 *        a threshold which is too low (why?).
 */
#define DISABLE_CONFIRMATION_THRESHOLD


#endif /* NGENE_CONFIGURATION_H_INCLUDED */
