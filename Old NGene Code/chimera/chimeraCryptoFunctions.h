/*
 * cryptoFunctionsTest.h
 *
 *  Created on: May 16, 2019
 *      Author: micaela
 */

#ifndef CHIMERA_CRYPTOFUNCTIONS_H_
#define CHIMERA_CRYPTOFUNCTIONS_H_
//
#include "chimeraDataStructures.h"


//#define DEBUG


#define handleErrors abort

/**
 * @def CHIMERA_DS_SIZE_BYTES
 * @brief This symbol defines the size of the Chimera Digital Signature (bytes)
 */
#define CHIMERA_DS_SIZE_BYTES 56

/**
 * @def CHIMERA_DS_SIZE_BITS
 * @brief This symbol defines the size of the Chimera Digital Signature (bits)
 */
#define CHIMERA_DS_SIZE_BITS 448

/**
 * @def CHIMERA_MESSAGE_SIZE_BITS
 * @brief This symbol defines the size of the Chimera Messagge for the Digital Signature verification (bytes)
 */
#define CHIMERA_MESSAGE_SIZE_BITS 7902

/**
 * @def CHIMERA_MESSAGE_SIZE_BYTES
 * @brief This symbol defines the size of the Chimera Messagge for the Digital Signature verification (bytes)
 */
#define CHIMERA_MESSAGE_SIZE_BYTES 988

/**
 * @def NUMBER_OF_MESSAGE_IN_A_EPOCH
 * @brief This symbol defines the number of message in a Chimera Epoch
 */
#define NUMBER_OF_MESSAGE_IN_A_EPOCH 10

/**
 * @def #define NUMBER_OF_BYTES_IN_A_MESSAGE 

 * @brief This symbol defines the number of bytes in a message
 */
#define NUMBER_OF_BYTES_IN_A_MESSAGE 111

/**
 * @def #define NUMBER_OF_BITS_IN_A_MESSAGE 

 * @brief This symbol defines the number of bits in a message
 */
#define NUMBER_OF_BITS_IN_A_MESSAGE 883


/**
 * @def #define BIT_INDEX_START_SUBFRAME3_IN_A_MESSAGE 

 * @brief This symbol defines the bit index of the start of the SUBFRAME 3 in each message of a Chimera Epoch
 */
#define BIT_INDEX_START_SUBFRAME3_IN_A_MESSAGE 609


/**
 * @def #define BIT_INDEX_TOI_FIELD_IN_A_MESSAGE 

 * @brief This symbol defines the bit index of the field TOI in each message of a Chimera Epoch
 */
#define BIT_INDEX_TOI_FIELD_IN_A_MESSAGE 0

/**
 * @def #define BIT_INDEX_PRN_FIELD_IN_A_MESSAGE 

 * @brief This symbol defines the bit index of the field PRN in each message of a Chimera Epoch
 */
#define BIT_INDEX_PRN_FIELD_IN_A_MESSAGE 609

/**
 * @def #define BIT_INDEX_PAGE_NUMBER_FIELD_IN_A_MESSAGE 

 * @brief This symbol defines the bit index of the field PAGE NUMBER in each message of a Chimera Epoch
 */
#define BIT_INDEX_PAGE_NUMBER_FIELD_IN_A_MESSAGE 617


/**
 * @def #define LENGTH_TOI_FIELD_IN_A_MESSAGE 

 * @brief This symbol defines the length  in bits of the field TOI in each message of a Chimera Epoch
 */
#define LENGTH_TOI_FIELD_IN_A_MESSAGE 9


/**
 * @def #define LENGTH_WN_FIELD_IN_A_MESSAGE 

 * @brief This symbol defines the length  in bits of the field WN in the Subframe 2 of each message of a Chimera Epoch
 */
#define LENGTH_WN_FIELD_IN_A_MESSAGE 13

/**
 * @def #define LENGTH_ITOW_FIELD_IN_A_MESSAGE 

 * @brief This symbol defines the length  in bits of the field ITOW in the Subframe 2 of each message of a Chimera Epoch
 */
#define LENGTH_ITOW_FIELD_IN_A_MESSAGE 8

/**
 * @def #define LENGTH_PRN_FIELD_IN_A_MESSAGE 

 * @brief This symbol defines the length  in bits of the field PRN in each message of a Chimera Epoch
 */
#define LENGTH_PRN_FIELD_IN_A_MESSAGE 8


/**
 * @def #define LENGTH_PAGE_NUMBER_FIELD_IN_A_MESSAGE 

 * @brief This symbol defines the length  in bits of the field PAGE NUMBER in each message of a Chimera Epoch
 */
#define LENGTH_PAGE_NUMBER_FIELD_IN_A_MESSAGE 6


/**
 * @def #define TWO_HOUR_PERIOD_DURATION 

 * @brief This symbol defines the duration in seconds of a 2-hours period, that is the unit of the Interval Time Of Week (ITOW)
 */
#define TWO_HOUR_PERIOD_DURATION 7200


/**
 * @def #define MESSAGE_DURATION 

 * @brief This symbol defines the duration in seconds of one message in a Chimera Epoch 
 * */
#define MESSAGE_DURATION 18

/**
 * @def #define PILOT_DURATION 

 * @brief This symbol defines the duration in milliseconds of one pilot symbol 
 * */
#define PILOT_DURATION 10


/**
 * @def #define NUMBER_OF_PILOT_SYMBOLS_IN_A_EPOCH 

 * @brief This symbol defines the number of pilot symbol in one Chimera Epoch
 * */
#define NUMBER_OF_PILOT_SYMBOLS_IN_A_EPOCH 1800

/**
 * @def #define MAX_NUMBER_OF_FAST_KEYS_IN_ONE_EPOCH 

 * @brief This symbol defines the maximum number of fast marker keys in one Chimera Epoch
 * */
#define MAX_NUMBER_OF_FAST_KEYS_IN_ONE_EPOCH 120

/**
 * @def #define MAX_NUMBER_OF_BLOCK_FOR_INDICATOR 

 * @brief This symbol defines the maximum number of 128-bits ciphertext block used to compute the marker locations
 * */
#define MAX_NUMBER_OF_BLOCK_FOR_INDICATOR 3

/**
 * @def #define MAX_NUMBER_OF_BLOCK_FOR_VALUE 

 * @brief This symbol defines the maximum number of 128-bits ciphertext block used to compute the marker values
 * */
#define MAX_NUMBER_OF_BLOCK_FOR_VALUE 16



/**
 * @def #define SECTOR_PATTERNS_ENTRIES 

 * @brief This symbol defines the number of rows and columns of Sector Patterns Table, define at pag. 32 of the ICD version 17/04/2019
 * */
#define SECTOR_PATTERNS_ENTRIES 31

/**
 * @def #define MAX_NUMBER_OF_SEGMENTS_FOR_LOCATION 

 * @brief This symbol defines the maximum number of segments used for marker locations for each sector (1 ms), defined in Table 20-4 at pag. 33 of the ICD version 17/04/2019
 * */
#define MAX_NUMBER_OF_SEGMENTS_FOR_LOCATION 7

/**
 * @def #define MAX_NUMBER_OF_MARKER_FOR_PILOT 

 * @brief This symbol defines the maximum number of markers values for each pylot symbol (10 ms), defined in Table 20-5 at pag. 36 of the ICD version 17/04/2019
 * */
#define MAX_NUMBER_OF_MARKER_FOR_PILOT 2030

/**
 * @def #define NUMBER_OF_CHIPS_PER_PILOT 

 * @brief This symbol defines the number of chips for each pylot symbol (10 ms)
 * */
#define NUMBER_OF_CHIPS_PER_PILOT 10230

void ReadHexBytes(char *hexArray,unsigned char *byteArray,int num);


int InitChimeraParameters(TChimeraParameters *chimeraPars,char *directory);

int InitChimeraData(TChimeraData *chimera);

void DeinitChimeraData(TChimeraData *chimera);

int Digital_SignatureVerification_fromEVPKEY(unsigned char *epoch,TChimeraData *chimera, int *toi);

void SlowChannelMarkerKeyGeneration(unsigned char *digitalSignature,TChimeraData *chimera, int prn);

int AESEncrypt(unsigned char *plaintext, int textLength, unsigned char *markerKey, unsigned char *ciphertext);

int AESDecrypt(unsigned char *ciphertext, int ciphertextLen, unsigned char *markerKey, unsigned char *plaintext);

void BuildPlaintext(TChimeraData *chimera, TMarkerFunctionEnum function, unsigned char ciphertextBlockIndex, int pilotSymbolIndex, int indexMsg,TChimeraChannelEnum channel);


void MarkersGeneration(TChimeraData *chimera, TChimeraChannelEnum channel, int prn, int pilotSymbolsToGenerate);


int LoadSectorPatternTable(TChimeraData *chimera);

void GeneratePatternList(TChimeraData *chimera);

void CiphertextGeneration(TChimeraData *chimera, int ciphertextBlockNeeded, TMarkerFunctionEnum function, TChimeraChannelEnum channel,int pilotSymbolIndex, int indexMsg, int prn);

#ifdef DEBUG
void LocationsListGeneration(TChimeraData *chimera, TChimeraChannelEnum channel,int ciphertextBytes, int numberOfSegmentsPerSector, int pilotSymbolIndex);
#else
void LocationsListGeneration(TChimeraData *chimera, TChimeraChannelEnum channel,int ciphertextBytes, int numberOfSegmentsPerSector);
#endif

void ValuesListGeneration(TChimeraData *chimera, int ciphertextBytesForValue,int ciphertextResidualBitForValue);

int LoadFastMarkerKey(TChimeraData *chimera);

void MarkersLocationsGeneration(TChimeraData *chimera, TChimeraChannelEnum channel, int prn, int numberOfciphertextBlocksForLocation, int pilotSymbolIndex, int indexMsg, int ciphertextBytesForLocation, int numberOfSegmentsPerSector);

#ifdef CHIMERA_PROFILING
void MarkersValuesGeneration(TChimeraData *chimera, TChimeraChannelEnum channel, int prn, int numberOfciphertextBlocksForValue, int pilotSymbolIndex, int indexMsg, int ciphertextBytesForValue, int ciphertextResidualBitForValue, int numberOfSegmentsPerSector);
#else
void MarkersValuesGeneration(TChimeraData *chimera, TChimeraChannelEnum channel, int prn, int numberOfciphertextBlocksForValue, int pilotSymbolIndex, int indexMsg, int ciphertextBytesForValue, int ciphertextResidualBitForValue);
#endif

//void OneCiphertextBlockGeneration(TChimeraData *chimera, int ciphertextBlockIndex, TMarkerFunctionEnum function, TChimeraChannelEnum channel,int pilotSymbolIndex, int indexMsg, unsigned char *markerKey,unsigned char *markerBlock);
#endif /* CHIMERA_CRYPTOFUNCTIONS_H_ */
