/**
 * @file guiInterfaceFunctions.h
 *
 * @author Mario Nicola
 *
 * @date 11/08/2014
 *
 * @version 1.0.0
 *
 * @brief Functions that manage the communication between Java GUI and C receiver.
 */

#ifndef GUI_INTERFACE_FUNCTIONS_H_INCLUDED
#define GUI_INTERFACE_FUNCTIONS_H_INCLUDED

#include "ngeneConfiguration.h"
#include "ngeneConstants.h"
#include "ngeneDataStructure.h"


/**
 * @struct DataToGuiHeaderStruct
 *
 * @brief DataToGuiHeaderStruct contains some outputs for the GUI, namely all values that are in common
 * to all the channels.
 *
 * @details This structure is not used. It is declared as
 * 			a documentation about the order of the outputs to the GUI.
 */
typedef struct
{
    int txtMsgLength;			/**< actual length of msg (in characters) */
    char msg[MAX_MSG_LENGTH];	/**< array containing a text message to be sent to GUI (now it's a fake message) */
    int validPvt;				/**< flag indicating validity of PVT solution sent to GUI. If its value is 0,
    							  	 then actual position is not valid or has been already sent to the GUI. Client
    							  	 must use this flag in order to to correctly perform both visualization of
    							  	 positions and statistical analysis */
    int gpsTime;				/**< Actual GPS time as obtained by navigation module */
    int galileoTime;			/**< Actual Galileo time as obtained by navigation module */
    int utcTime;				/**< Actual UTC time as obtained by navigation module */
    int gpsWeek;				/**< Actual value of GPS week. The value read in the navigation message of
     	 	 	 	 	 	 	 	 reference channel is used (i.e., the channel used as reference during pseudorange calculation). */
    int tow;				/**< Actual value of Time Of Week. The value read in the navigation message of reference channel is used (i.e., the channel used as reference during pseudorange calculation). */
    float lat;				/**< Latitude for receiver position (in geodetic coordinates). In decimal degree. */
    float lon;				/**< Longitude for receiver position (in geodetic coordinates). In decimal degree. */
    float alt;				/**< Altitude for receiver position (in geodetic coordinates). In meters. */
    float velLat;			/**< North-south component for receiver speed (in ENU coordinates). In meters per second. */
    float velLon;			/**< East-west component for receiver speed (in ENU coordinates). In meters per second. */
    float velAlt;			/**< Altitude component for receiver speed (in ENU coordinates). In meters per second. */
    float ecefX;			/**< X component for receiver position (in ECEF coordinates). In meters */
    float ecefY;			/**< Y component for receiver position (in ECEF coordinates). In meters */
    float ecefZ;			/**< Z component for receiver position (in ECEF coordinates). In meters */
    float enuE;				/**< East-west component for receiver position (in ENU coordinates). In meters */
    float enuN;				/**< North-south component for receiver position (in ENU coordinates). In meters */
    float enuU;				/**< Altitude component for receiver position (in ENU coordinates). In meters */
    float gdop;				/**< Geometric Diluition Of Precision */
    float hdop;				/**< Horizontal Diluition Of Precision */
    float vdop;				/**< Vertical Diluition Of Precision */
    float pdop;				/**< Position Diluition Of Precision */
    int gpsSubframe;		/**< Index of the last subframe being received by reference channel (i.e., the channel used as reference during pseudorange calculation). For GPS prns. */
    int gpsPage;			/**< Index of the last page being received by reference channel (i.e., the channel used as reference during pseudorange calculation). For GPS prns. */
    int galileoSubframe;	/**< Actual Galileo subframe */
    int galileoPage;		/**< Actual Galileo Page */
    int useEgnos;			/**< If useEGNOS = 1, then EGNOS messages are used to correct positioning. (it's equal to analogous input parameter) */
    long long sampleNumber;	/**< Total number of discarded samples: it roughly indicates how much time has been passed from the start of elaboration, and it's used as a tag at the client side. */
    int nChannels;			/**< Number of supported channels (it's equal to analogous input parameter) */
} TDataToGuiHeader;


/**
 * @struct DataToGuiChannelStruct
 *
 * @brief Contains some outputs for the GUI, namely all values that are
 * specific for a given channel.
 *
 * @details When all outputs are prepared and sent to client, a different DataToGuiChannelStruct
 * structure will be sent for each channel.
 */
typedef struct DataToGuiChannelStruct
{
    int prnLength;				/**< Number of characters used to represent name of prn allocated to current channel */
    char prn[PRN_NAME_LENGTH];	/**< Array of characters with the name of prn allocated to the current channel.
   									 It contains both constellation and index of the prn, e.g. GPS1 */
    int channelIndex;			/**< Index of the current channel in the channel array */
    int statusLength;			/**< Number of characters used to represent the status of the current channel */
    char status[STATUS_LENGTH];	/**< String that represents the current state of the channel (see state variable
  									 in the channel structure) */
    float doppler;				/**< Last estimation of Doppler frequency for actual channel (from tracking)*/
    float cn0;					/**< Last estimation of CN0 for actual channel (from tracking)*/
    int lockStatus;				/**< The value is equal to 1 when signal is locked, else it is equal to 0.
   									 For display purposes, the signal of a prn is locked when the channel
   									 which is allocated to is in the NAVIGATION_STATE. */
    float pseudorangePrecision;	/**< Actually a default value is set for this variable */
    float accumDopplerPrecision;/**< Actually a default value is set for this variable */
    float satElevation;			/**< Elevation of the satellite allocated to the current channel, as determined
  									 by PVT routines. It's expressed in radiants. */
    float satAzimuth;			/**< Azimuth of the satellite allocated to the current channel, as determined
  									 by PVT routines. It's expressed in radiants. */
    float pseudorange;			/**< Distance between satellite and receiver, as determined by
  									 by PVT routines. It's expressed in meters.*/
    float earlyCorr;			/**< Last in-phase component of early correlation calculated by tracking module
   									 (with sign and normalized dividing it by intPeriod) */
    float promptCorr;			/**< Last in-phase component of prompt correlation calculated by tracking module
   									 (with sign and normalized dividing it by intPeriod) */
    float lateCorr;				/**< Last in-phase component of late correlation calculated by tracking module
   									 (with sign and normalized dividing it by intPeriod) */
    int tracked;				/**< Its value is 1 when current state is in the TRACKING_STATE or in the NAVIGATION_STATE */
    int usedForPVT;				/**< Its value is 1 when current state is in the NAVIGATION_STATE */
    int healthySat;				/**< This is the health value read from navigation message */
    float satEcefX;				/**< X component for satellite position (in ECEF coordinates). In meters  */
    float satEcefY;				/**< Y component for satellite position (in ECEF coordinates). In meters  */
    float satEcefZ;				/**< Z component for satellite position (in ECEF coordinates). In meters  */
} TDataToGuiChannel;



/**
 * @brief Parses the configuration received from the GUI.
 *
 * @param guiCfg memory containing the configuration from the GUI
 * @param rxCfg points to the receiver configuration
 *
 * @return SUCCESS_RETURN when the desired value is read, FAILURE_RETURN otherwise
 */
int LoadGuiConfigurationIntoReceiver(void *dataFromGui,TNGeneConfiguration *rxCfg);

/**
 * @brief Writes the data to be sent to the GUI.
 *
 * #details If the memory allocated in buffer is not enough, the memory is freed and
 *          a larger memory location is allocated.
 *
 * @param bufferSize points to the current size of buffer
 * @param buffer points to the memory that will contain the results.
 * @param rxCfg is the structure containing the receiver configuration
 * @param rx is the structure that contains the receiver data
 *
 * @return SUCCESS_RETURN when the desired value is read, FAILURE_RETURN otherwise
 */
int UpdateDataPacket(int *bufferSize,void **buffer,TNGeneConfiguration *rxCfg,TNGeneData *rx);

#endif /* GUI_INTERFACE_FUNCTIONS_H_INCLUDED */
