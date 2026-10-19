/**
 * @file osnmaUtilities.h
 *
 * @author Mario Nicola
 *
 * @date 12/04/2019
 *
 * @version 1.0.0
 *
 * @brief some easy functions to parse and manage OSNMA
 */

#ifndef OSNMA_UTILITIES_H_INCLUDED
#define OSNMA_UTILITIES_H_INCLUDED

#include "osnmaDataStructures.h"
#include "spreadingCodes.h"

/**
 * @brief Translate from DSM ID to the type of the current DSM according to Table 5, page 10
 *
 * @param dsmId is the DSM ID (DSID?) as reported in DSM Header of the HKROOT section
 *
 * @return The type of the current DSM
 */
TDsmType GetDsmTypeFromDsmId(int dsmId);


/**
 * @brief Get the number of blocks from the NB field in DSM_KROOT (Table 8, page 11) and DSM_PKR (page 15)
 *
 * @param nb is the code read from the navigation message
 *
 * @return The number of blocks; -1 if invalid
 */
int GetNumberOfBlocks(int nb);


/**
 * @brief Get the key size from the KS field in DSM_KROOT (Table 12, page 12)
 *
 * @param ks is the code read from the navigation message
 *
 * @return The size of the TESLA chain keys (in bits); -1 if invalid
 */
int GetTeslaChainKeySize(int ks);


/**
 * @brief Get Hash Function to be used for TESLA Key verification
 *
 * @param osnma is pointer to the OSNMA parameters structure
 *
 * @return void
 */
void GetHashFunctionForTeslaKeyVerification(TOsnmaParameters *osnma);


/**
 * @brief Get the MAC size from the MS field in DSM_KROOT (Table 13, page 13)
 *
 * @param ms is the code read from the navigation message
 *
 * @return The size of the MAC (in bits); -1 if invalid
 */
int GetMacSize(int ms);


/**
 * @brief Get Sat Type and PRN in function of the PRN field from OSNM message (Table 19, page 18)
 *
 * @param osnmaPrn is the PRN reported in the OSNMA message
 * @param satTypePtr points to the corresponding constellation
 * @param prnPtr points to the corresponding PRN
 */
void GetSatTypePrnFromOsnmaPrn(int osnmaPrn,TSatConstellation *satTypePtr,int *prnPtr);


/**
 * @brief Get Sat Type and PRN in function of the PRN field from OSNM message (Table 19, page 18)
 *
 * @param satType is the constellation
 * @param prn is the PRN
 *
 * @return osnmaPrn, i.e. the PRN coded in the OSNMA message
 */
int GetOsnmaPrnFromSatTypePrn(TSatConstellation satType,int prn);


/**
 * @brief Check if the PRN is transmitted with the MACK Offset or not
 *        (According to the ICD 2.1 definition)
 *
 * @param prn is the PRN
 * @param mackOffsetEnabled MACK Offset flag
 * @param ns is the NS parameter
 *
 * @return TRUE_FLAG if the PRN is offset, FALSE_FLAG otherwise
 */
int IsThisPrnOffset(int prn,int mackOffsetEnabled,int ns);


#ifdef MACK_OFFSET_ICD2_DEFINITION
/**
 * @brief Get the MACK offset as reported by Section 6.9 page 33
 *
 * @param prn is the satId for the current channel
 * @param mackOffsetEnabled MACK Offset flag
 * @param nmack is the number of MACK blocks in a subframe (valid values: 1, 2, 3)
 * @param ns is the NS parameter
 *
 * @return the MACK offset in seconds (valid for Galileo PRN 19 and beyond)
 */
double GetMackOffsetSeconds(int prn,int mackOffsetEnabled,int nmack,int ns);
#else
/**
 * @brief Get the MACK offset as reported by Section 6.9 page 33
 *
 * @param prn is the satId for the current channel
 * @param mackOffsetEnabled MACK Offset flag
 * @param nmack is the number of MACK blocks in a subframe (valid values: 1, 2, 3)
 *
 * @return the MACK offset in seconds (valid for Galileo PRN 19 and beyond)
 */
double GetMackOffsetSeconds(int prn,int mackOffsetEnabled,int nmack);
#endif

/**
 * @brief Get the Start time of the current Galileo subframe, starting from a Galileo time
 *
 * @param tow is a Galileo Time Of Week [s]
 * @param offsetPtr points to the offset of tow w.r.t. the start time of the subframe
 *
 * @return the start time of the subframe.
 *         For E1, this is the E1 sub-frame start time minus 1 second (see page 26)
 */
int GetGalE1bSubframeStartTime(int tow,int *offsetPtr);


/**
 * @brief Get the PRN corresponding to the key in the TESLA chain with given index
 *
 * @param maxNs is the maximum number of satellites that can be used as NS paramenter
 * @param ns is the number of satellites
 * @param keyIndexTable is pointer to the LUT that, given the TESLA key index, provides as output the PRN according to the current OSNMA parameters
 * @param indexKey is the index of the current key in the TESLA chain (see ICD Figure 5, page 26)
 * @param prnIndex index of the PRN (i.e. in each MACK block we can have more than one PRN
 *        with the same key, namely up to 36/NS. prnIndex goes from 1 to 36/NS and
 *        indicates which of the PRNs with the same key will be returned
 *
 * @return one of the PRNs corresponding to the current key
 */
int FromTeslaChainKeyIndexToPrn(int maxNs, int ns,int *keyIndexTable, int indexKey,int prnIndex);



/**
 * @brief Get the subframe start time corresponding to the key in the TESLA chain with given index
 *
 * @param nmack is the number of mack blocks per subframe
 * @param ns is the number of satellites
 * @param krootTow is the TOW of the ROOT key
 * @param indexKey is the index of the current key in the TESLA chain (see ICD Figure 5, page 26)
 *
 * @return the start time of the subframe.
 *         For E1, this is the E1 sub-frame start time minus 1 second (see page 26)
 */
int FromTeslaChainKeyIndexToTime(int nmack, int ns, int krootTow, int indexKey);


/**
 * @brief Check if the specified key is used to validate MAC for PRNs with MACK Offset
 *        (According to the ICD 2.1 definition)
 *
 * @param mackOffsetEnabled MACK Offset flag
 * @param ns is the NS parameter
 * @param indexKey is the index of the current key in the TESLA chain
 *        (see ICD 2.1, Section 5.4.2, pages 39, 40)
 *
 * @return TRUE_FLAG if indexKey indicates a key used to validate MACs from PRNs with MACK Offset
 */
int IsThisKeyForMackOffset(int mackOffsetEnabled,int ns,int indexKey);

#endif /* OSNMA_UTILITIES_H_INCLUDED */
