/**
 * @file ngeneDataStructure.h
 *
 * @author Mario Nicola
 *
 * @date 12/08/2014
 *
 * @version 1.0.0
 *
 * @brief Declares the basic data structure of the receiver.
 */

#ifndef NGENE_DATA_STRUCTURE_H_INCLUDED
#define NGENE_DATA_STRUCTURE_H_INCLUDED

#include "ngeneConstants.h"
#include "circularBuffer.h"
#include "prnTable.h"
#include "carrierTable.h"
#include "fft.h"
#include "acquisitionDataStructure.h"
#include "channelDataStructure.h"
#include "warmStartDataStructure.h"
#include "navigationMessageDataStructure.h"
#include "egnosCorrectionsDataStructure.h"
#include "pvtDataStructure.h"
#include "samplesHistogram.h"
#include "samplesSpectrum.h"
#include "ncursesDataStructure.h"
#include "rinexDataStructure.h"
#include "osnmaDataStructures.h"
#include "osnmaValidationsDataStructures.h"
#include "logSplitterDataStructure.h"

/**
 * @def NGENE_ARRAYS_SIZE
 * @brief The size of the arrays in the NGene structure (despreading code, carrier) in ms
 */
#define NGENE_ARRAYS_SIZE 	2


/**
 * @struct NGeneDataStruct
 *
 * @brief Contains all the basic data for the NGene receiver
 */
typedef struct NGeneDataStruct
{
	int dataValid;						/**< if FALSE_FLAG, the de-initialization function has already been executed */
	/**/
	void *bytesFromGui;					/**< memory used to write parameters received from the GUI. NULL if GUI is not selected */
	int bytesToGuiSize;					/**< size (n. of bytes) of bytesToGui. Zero if GUI is not selected */
	void *bytesToGui;					/**< memory used to write data to be transmitted to the GUI. NULL if GUI is not selected */
	char msgToGui[MAX_ROW_LENGTH];		/**< next text message to be sent to the GUI */
	char msgToGuiOld[MAX_ROW_LENGTH];	/**< last text message already sent to the GUI */
	/**/
	unsigned long long int rxCounter;   /**< counts the execution cycles of the receiver (about, the number of ms from the start ) */
	char *codeArray;					/**< contains the despreading code at fs */
	char *codeEarlyArray;               /**< contains the despreading code early replica, at fs (low fs only) */
	char *codeLateArray;                /**< contains the despreading code late replica, at fs (low fs only) */
	char *cosArray;						/**< contains the cosine carrier at fs */
	char *sinArray; 					/**< contains the sine carrier at fs */
	char *subcarrierArray; 				/**< contains the subcarrier used to wipe-off the BOC(1,1) in Galileo */
	char *subcarrierArrayEarly; 		/**< contains the subcarrier used to wipe-off the BOC(1,1) in Galileo (Trk Early replica with low fs) */
	char *subcarrierArrayLate; 			/**< contains the subcarrier used to wipe-off the BOC(1,1) in Galileo (Trk Late replica with low fs) */
	TCircularBuffer buf;                /**< buffer storing input samples */
	TPrnTable prnTable; 				/**< table used to generate despreading codes at fs */
	TSinCosTable fifTable;        		/**< table used to generate carrier */
	TSinCosTable fscTable; 				/**< table used to generate the subcarrier for Galileo E1b */
	TFftTable fftTable;					/**< table used to perform FFT and IFFT */
	TAcqCommon acqCommon;				/**< structure used to store acquisition variables common to all the channels */
	TChannelScheduler channelScheduler;	/**< structure used to manage channel allocation */
	TChannel *channelArray; 			/**< points to all the channels */
	/* Warm Start */
	TWarmStartData warmStartData;       /**< data used to manage the optional warm start */
	/* Navigation message */
	int gpsLastDecodedPage; 			/**< index of the page for the last decoded GPS subframe */
	int gpsLastDecodedSubframe;  		/**< index of the last decoded GPS subframe */
	TNavigationMessage *gpsNavigationMessage;	/**< The navigation message (and almanac) for all the supported GPS PRNs */
	int gpsIonoValid;					/**< TRUE_FLAG when gpsIono is valid */
	TGnssIono gpsIono;					/**< stores the ionosphere correction in a data structure common to all GPS satellites */
	/**/
	int galLastDecodedPage; 			/**< index of the subframe for the last decoded Galileo word */
	int galLastDecodedSubframe;  		/**< index of the last decoded Galileo word */
	TNavigationMessage *galNavigationMessage;	/**< The I/NAV navigation message (and almanac) for all the supported Galileo PRNs */
	TNavigationMessage *fnavNavigationMessage;	/**< The F/NAV navigation message (and almanac) for all the supported Galileo PRNs */
	int galIonoValid;					/**< TRUE_FLAG when galIono is valid */
	TGnssIono galIono;					/**< stores the ionosphere correction in a data structure common to all Galileo satellites */
	/**/
	TEgnosCorrections egnosCorrections;	/**< Stores the corrections read from the EGNOS message */
	/* OSNMA */
	TOsnmaData osnmaData;               /**< All variables used to manage OSNMA */
	TOsnmaAuthentication osnmaAuth;     /**< All variables used to manage the authentications obtained through OSNMA */
	/* PVT computation */
	TPvtData pvtData; 					/**< This structure contains all the information related to PVT computation */
	TPvtResults pvtTemporaryResults; 	/**< This structure contains the results of the PVT before the check of their goodness is performed */
	TPvtResults pvtResults; 			/**< This structure contains the results of the PVT computation */
	/* Other logs */
	TLogSplitter logSplitter;           /**< Used to manage the splitting of the receiver logs */
	TSamplesHistogram samplesHistogram; /**< This structure is used to evaluate the histogram of the input samples */
	TSamplesSpectrum samplesSpectrum; 	/**< This structure is used to evaluate the spectrum of the input samples */
	char *ephemeridesPacketToGui;		/**< These bytes store the ephemerides packet to be sent to the GUI */
	FILE *trkCorrelatorsFile; 			/**< File where all the correlations from tracking are saved */
	FILE *trkMultiCorrelatorsFile;      /**< File where all the multiple correlations from tracking are saved */
	TRinexLog rinexGps;					/**< Variable used to save the RINEX navigation file for the GPS navigation message (and ALL the observation data) */
	TRinexLog rinexGal;					/**< Variable used to save the RINEX navigation file for the Galileo navigation message */
	FILE *nmeaFile;                     /**< File where the NMEA sentences are saved */
	char *trkLogArray;                  /**< Array used to write the tracking related logs */
	FILE *ifenLogGps;                   /**< File with the log of the GPS navigation message in IFEN format */
	long long int ifenLogGpsTime;       /**< Time of the first subframe saved in ifenLogGps (WN+TOW in seconds) */
	FILE *ifenLogGal;                   /**< File with the log of the Galileo navigation message in IFEN format */
	long long int ifenLogGalTime;       /**< Time of the first page part saved in ifenLogGal (WN+TOW in seconds) */
	FILE *ifenLogFNav;                  /**< File with the log of the Galileo F/NAV navigation message in IFEN format */
	long long int ifenLogFNavTime;      /**< Time of the first page part saved in ifenLogFNav (WN+TOW in seconds) */
	TNCursesData ncursesData;			/**< Variables used to manage the NCurses interface */
	#ifdef RX_PROFILING
	TProfilingSupport pvtProfiling; /**< profiling of the PVT */
	int pvtTime;					/**< flag indicating if it's time for a new PVT computation*/
	#endif
} TNGeneData;



#endif /* NGENE_DATA_STRUCTURE_H_INCLUDED */
