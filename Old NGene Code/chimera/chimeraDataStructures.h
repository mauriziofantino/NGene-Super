/**
 * @file chimeraDataStructures.h
 *
 * @author Micaela Troglia Gamba
 *
 * @date 28/05/2020
 *
 * @version 1.0.0
 *
 * @brief data structures used to manage Chimera
 *        References to
 *        Chimer Specification v1.1
 */
#ifndef CHIMERA_DATA_STRUCTURES_H_INCLUDED
#define CHIMERA_DATA_STRUCTURES_H_INCLUDED

#include <stdio.h>
#include "osnmaConstants.h"
#include "spreadingCodes.h"
#include "ngeneConstants.h"
#include <openssl/ossl_typ.h>
#include "megaBuffer.h"
#include <bits/pthreadtypes.h>
#include "profilingSupport.h"


/**
 * @enum ChimeraChannelEnum
 * @brief Chimera channel
 */
typedef enum ChimeraChannelEnum
{
	SLOW_CHANNEL = 0, /**< SLOW  Channel */
	FAST_CHANNEL = 1 /**< FAST Channel */
} TChimeraChannelEnum;

/**
 * @enum MarkerFunctionEnum
 * @brief Chimera Marker Function
 */
typedef enum MarkerFunctionEnum
{
	MARKER_LOCATION = 0, /**< Marker Locations generation */
	MARKER_VALUE = 1 /**< Marker Values generation */
} TMarkerFunctionEnum;


/**
 * @struct ChimeraParametersStruct
 *
 * @brief .
 */
typedef struct ChimeraParametersStruct
{
		
    int MSizeBits;/**< size of Message M to verify the Digital Signature Verification [bits] */
	int MSizeBytes;/**< size of Message M to verify the Digital Signature Verification [bytes] */
	int DSSizeBytes;/*Size of the Digital Signature [bytes]*/
    int DSSizeBits;/*Size of the Digital Signature [bits]*/
    int DSHashedSizeBytes;/*Size of the Hashed value of the Digital Signature [bytes]*/
    int markerKeySizeBytes;/*Size of the marker keys [bytes]*/
    int markerKeySizeBits; /*Size of the marker keys [bits]*/

    int textSizeBytes;/*Size of the plaintext and ciphertext [bytes]*/
    int zCountSizeBytes;/*Size of the GPS Z-Count [bytes]*/

	char publicKeyFilename[2*MAX_ROW_LENGTH];
    char fastMarkerKeysFilename[2*MAX_ROW_LENGTH];
    FILE *fastMarkerKeyFile;
} TChimeraParameters;

/**
 * @struct ChimeraDataStruct
 *
 * @brief .
 */
typedef struct ChimeraDataStruct
{
    TChimeraParameters chimeraPars;    /**< OSNMA parameters */

	EVP_PKEY *evp_key;/*Public Key (internal format for OPEN SSL Library functions)*/
	const EVP_MD *function; /*Hash function used to generate the DSM-KROOT Digital Signature*/
	unsigned char *M; /*Message to be signed, as at page 24-25 of ICD 17/04/2019*/
	unsigned char *DS; /*Received Digital Signature*/
    
    unsigned char *DSHashed; /*Hashed of DS*/
    unsigned char **slowMarkerKey;/*Slow Marker Keys, one per PRN*/
    unsigned char **fastMarkerKeyList;/*Fast Marker Keys in one Chimera Epoch*/
    unsigned char *fastMarkerKey;/*Fast Marker Key at each fast period*/
    int indexFastKey;/*Index of the Fast Marker Key in the Chimera Epoch*/

    
    unsigned char *plaintext;/*Plaintext*/
    unsigned char *ciphertext;/*ciphertext*/
    unsigned char prn; /*PRN*/
    unsigned char fastEpochPeriod; /*Period for the Fast Channel: 1.5 seconds (value 0) or 6 seconds (value 1)*/
    unsigned char fastDF; /*Fast Channel Duty Factor*/
    unsigned char slowDF; /*Slow Channel Duty Factor*/
    
    unsigned char **zCount;/*GPS Z-Count per each message in a Chimera Epoch*/
    unsigned char pilotSymbolIndexCharArray[2];
    
    unsigned char *markerIndicatorBlock;/*Marker Indicator Block*/
    unsigned char *markerValueBlock;/*Marker Value Block*/
    unsigned char **sectorPattern;/*Sector Patterns*/
    unsigned char slowPatternList[16];/*Slow Pattern List for the current PRN*/
    unsigned char fastPatternList[15];/*Fast Pattern List for the current PRN*/
    unsigned char *markerLocationListPerSector;
    unsigned char *occupiedMarkerLocationFlag;
    
    unsigned char *markerLocationListPerPilot;
    unsigned char *markerValuesPerPilot;
    unsigned char *chips; /*Pilot code Chips*/

    
    //**Profiling structures
    TProfilingSupport dsProfiling;
    TProfilingSupport slowKeyProfiling; 
    TProfilingSupport oneCiphertextProfiling;
    TProfilingSupport tenMarkerLocationProfiling;
    TProfilingSupport tenMarkerValueProfiling;
    TProfilingSupport markerGenerationPerPilotProfiling;
    TProfilingSupport markerLocationGenerationPerPilotProfiling;
    TProfilingSupport markerValueGenerationPerPilotProfiling;
    
} TChimeraData;



#endif /* CHIMERA_DATA_STRUCTURES_H_INCLUDED */
