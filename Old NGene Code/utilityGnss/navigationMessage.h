/**
 * @file navigationMessage.h
 *
 * @author Mario Nicola
 *
 * @date 06/10/2014
 *
 * @version 1.0.0
 *
 * @brief Some basic functions to manage navigation message.
 */

#ifndef NAVIGATION_MESSAGE_H_INCLUDED
#define NAVIGATION_MESSAGE_H_INCLUDED


void ResetEphemerides(TGnssEphemerides *eph);

void ResetClockCorrections(TGnssClk *clk);

/**
 * @brief Initializes the structure used to store the navigation message
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int InitNavigationMessage(TNGeneConfiguration *cfg,TNGeneData *rx);

/**
 * @brief De-initializes the structure used to store the navigation message
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 */
void DeinitNavigationMessage(TNGeneConfiguration *cfg,TNGeneData *rx);

/**
 * @brief Copies ephTmp into eph and clkTmp into clk
 *
 * @param navMsg points to the target navigation message
 */
void CopyFromTmpToOperative(TNavigationMessage *navMsg);

/**
 * @brief Save two YUMA files containing the almanacs for GPS and Galileo respectively
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 */
void SaveAlmanacIntoYumaFile(TNGeneConfiguration *cfg,TNGeneData *rx);

/**
 * @brief Save one XML file containing the almanac for Galileo
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 */
void SaveGalileoAlmanacIntoXmlFile(TNGeneConfiguration *cfg,TNGeneData *rx);

/**
 * @brief Look for one valid TOW in all tracked channels
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 * @param chIndex is the index of the channel that will get the TOW/WN
 * @param satType points to the constellation from which TOW and WN are taken
 * @param wn points to the WN (-1 if not valid)
 * param tow points to the TOW (-1 if not valid)
 */
void GetATransmissionTime(TNGeneConfiguration *cfg,TNGeneData *rx,int chIndex,TSatConstellation *satType,int *wn,double *tow);

#endif /* NAVIGATION_MESSAGE_H_INCLUDED */
