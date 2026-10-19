/**
 * @file channelDataStructure.h
 *
 * @author Mario Nicola
 *
 * @date 23/08/2014
 *
 * @version 1.0.0
 *
 * @brief Declares the basic data structure of one channel.
 */

#ifndef CHANNEL_DATA_STRUCTURE_H_INCLUDED
#define CHANNEL_DATA_STRUCTURE_H_INCLUDED

#include "acquisitionDataStructure.h"
#include "trackingDataStructure.h"
#include "prnListDataStructure.h"
#include "ngeneConstants.h"
#include "navigationMessageParser.h"
#include "osnmaDataStructures.h"

/**
 * @enum ChannelStateEnum
 *
 * @brief Is the type used by doChannel function to represent its state.
 *        Sequence of state depends on results obtained and type of the PRN assigned to current channel (GPS, EGNOS, Galileo).
 *        Each channel owns a different state.
 */
typedef enum ChannelStateEnum
{
    POWERON_STATE,              /**< Each channel starts elaboration from this state: here a PRN is assigned to the channel */
    COARSE_ACQUISITION_STATE,   /**< Here the CAF for the PRN assigned to the channel is evaluated */
    CODE_REFINEMENT_STATE,      /**< Here the code delay estimation from the coarse acquisition is refined */
    DOPPLER_REFINEMENT_STATE,   /**< Here the Doppler shift estimation from the coarse acquisition is refined */
    CONFIRMATION_STATE,         /**< Here the presence of the signal is confirmed through the M-on-N approach */
    TRACKING_STATE,             /**< Here signal of assigned PRN is tracked */
    UNUSED_STATE                /**< Channel in this state will not be scheduled during execution */
} TChannelState;


/**
 * @struct PvtInfoAbouChannelStruct
 * @brief This structure contains information about the use of the channel in the PVT computation
 */
typedef struct PvtInfoAbouChannelStruct
{
	int usedForPVT;		/**< Its value is TRUE_FLAG when the channel has been used in the last PVT computation */
	float satElevation;	/**< Elevation of the satellite allocated to the current channel, as determined
	  					     by PVT routines. It's expressed in radians. */
	float satAzimuth;	/**< Azimuth of the satellite allocated to the current channel, as determined
	  						 by PVT routines. It's expressed in radians. */
	double pseudorange;	/**< Distance between satellite and receiver, as determined by
	     					 by PVT routines. It's expressed in meters.*/
	float satEcefX;		/**< X component for satellite position (in ECEF coordinates). In meters  */
	float satEcefY;		/**< Y component for satellite position (in ECEF coordinates). In meters  */
	float satEcefZ;		/**< Z component for satellite position (in ECEF coordinates). In meters  */
} TPvtInfoAbouChannel;


/**
 * @struct ChannelStruct
 *
 * @brief This structure stores all variable specific to the single channel
 *        (i.e. variables needed to perform acquisition for a single satellite.
 */
typedef struct ChannelStruct
{
	int chIndex;				/**< Index of the current channel in the channel array */
	TSatConstellation satType;	/**< Constellation set for this channel (chose at initialization) */
	TSatSignal satSignal;       /**< Signal set for this channel (chose at initialization) */
	int satId;					/**< Index of the satellite assigned to this channel */
	char prnName[PRN_NAME_LENGTH]; /**< String representing the name of the PRN (satellite) */
	TChannelState chState;		/**< Current state of the channel */
	int codeLen;				/**< Length of a period of the current spreading code [chip] */
	int codeDuration;			/**< Duration of a period of the current spreading code [ms] */
	int bitDuration; 			/**< Duration of a bit in the navigation message [ms] */
	/* validation status */
	TValidationStatus dsmStatus; /**< Validity of the received DMS (Galileo only) */
	TValidationStatus keyStatus; /**< Validity of the received TESLA key (Galileo only) */
	TValidationStatus msgStatus; /**< Validity of the received navigation message */
	TValidationStatus macseqStatus; /**< Validity of the received MACSEQ field */
	/**/
	unsigned int bufferOffset; 	/**< Displacement in the input buffer */
	unsigned int blocksize;		/**< How many samples will be elaborated at this execution cycle */
	unsigned int blocksizeNext;	/**< How many samples will be elaborated at the next execution cycle */
	double carrierFrequency;	/**< Frequency of the carrier [Hz] */
	double dopplerShift;		/**< Difference between the current value of carrier frequency and the nominal value [Hz] */
	double carrierFrequencyOld; /**< Frequency of the carrier [Hz]. Old value. */
	double dopplerShiftOld;		/**< Difference between the current value of carrier frequency and the nominal value [Hz]. Old value. */
	double carrierPhase;		/**< Carrier phase used to generate the carrier at this execution cycle [rad] */
	double codeRate;		 	/**< Code rate [Hz] */
	TAverageWindow codeRateMean;/**< mean value of the code rate estimations */
	double codePhase;			/**< Code phase used to generate the despreading code at this execution cycle [chip] */
	double codePhaseFrac;		/**< Fractional part of codePhase [chip] */
	/**/
	TCoarseAcquisition coarse;	/**< Structure that stores the variables used for coarse acquisition */
	TRefinement refinement;		/**< Structure that stores the variables used for the code delay and Doppler shift refinement */
	TConfirmation confirmation;	/**< Structure that stores the variables used for the confirmation */
	TTracking tracking; 		/**< Structure that stores the variables used for the tracking */
	int newNavMsgInformation;	/**< Flag that indicates whenever a new navigation message has been received */
	TGpsParser *gpsParser;		/**< Structure that stores the variables used for the parse of the GPS navigation message */
	TGalParser *galParser;		/**< Structure that stores the variables used for the parse of the Galileo I/NAV navigation message */
	TFNavParser *fnavParser;	/**< Structure that stores the variables used for the parse of the Galileo F/NAV navigation message */
	TEgnosParser *egnosParser; 	/**< Structure that stores the variables used for the parse of the EGNOS navigation message */
	int egnosPrnMaskIndex; 		/**< Index of the current satellite in the PRN mask broadcasted by EGNOS */
	TPvtInfoAbouChannel pvtInfo;/**< Structure that stores information about the use of the channel in the PVT computation */
	int checkSidePeakTrackingFlag; /**< Flag used only for Galileo satellites which has been fast re-acquired, to check whether the tracking is locked to the main or the side peaks*/

	#ifdef RX_PROFILING
	TProfilingSupport gpsCoarseAcqProfiling; /**< profiling of the GPS coarse acquisition */
	TProfilingSupport galileoCoarseAcqProfiling; /**< profiling of the GALILEO coarse acquisition */
	TProfilingSupport gpsDopplerRefinementProfiling; /**< profiling of the GPS Doppler Refinement */
	TProfilingSupport galileoDopplerRefinementProfiling; /**< profiling of the GPS Doppler Refinement */
	TProfilingSupport gpsCodeRefinementProfiling; /**< profiling of the GPS Code Refinement */
	TProfilingSupport galileoCodeRefinementProfiling; /**< profiling of the GPS Code Refinement */
	TProfilingSupport gpsConfirmationProfiling; /**< profiling of the GPS Confirmation */
	TProfilingSupport galileoConfirmationProfiling; /**< profiling of the GALILEO Confirmation */
	TProfilingSupport gpsTrackingProfiling; /**< profiling of the GPS Tracking */
	TProfilingSupport galileoTrackingProfiling; /**< profiling of the GALILEO Tracking */
	TProfilingSupport gpsFftProfiling; /**< profiling of the GPS FFT */
	TProfilingSupport galileoFftProfiling; /**< profiling of the GALILEO FFT */
	TProfilingSupport gpsSampleCompressionProfiling; /**< profiling of the GPS Sample Compression */
	TProfilingSupport galileoSampleCompressionProfiling; /**< profiling of the GALILEO Sample Compression */
	#endif
} TChannel;


/**
 * @def MAXIMUM_NUMBER_OF_CHANNELS_IN_ACQUISITION
 * @brief This is the maximum number of channels simultaneously in acquisition
 * 		  If it is not defined, then a flexible strategy for the number of channels in acquisition is used.
 */
//#define MAXIMUM_NUMBER_OF_CHANNELS_IN_ACQUISITION		1

/**
 * @struct ChannelSchedulerStruct
 *
 * @brief This structure contains all the data needed to allocate a PRN to the channel,
 *        or release it.
 */
typedef struct ChannelSchedulerStruct
{
	TPrnList prnList;				/**< list of PRNs for channel allocation */
	int maximumNumberOfChannelsInAcquisition;	/**< Maximum number of channels in acquisition */
	int channelsInAcquisition;		/**< number of channels in acquisition */
	int channelsInTracking;			/**< number of channels in tracking */
	int gpsChannelsInTracking;		/**< how many GPS channels are in tracking */
	int egnosChannelsInTracking;	/**< how many EGNOS channels are in tracking */
	int galileoChannelsInTracking;	/**< how many Galileo channels are in tracking */
} TChannelScheduler;

#endif /* CHANNEL_DATA_STRUCTURE_H_INCLUDED */
