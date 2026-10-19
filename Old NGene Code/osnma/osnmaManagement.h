/**
 * @file osnmaManagement.h
 *
 * @author Mario Nicola
 *
 * @date 16/08/2014
 *
 * @version 1.0.0
 *
 * @brief Some basic functions for the OSNMA management.
 */

#ifndef OSNMA_MANAGEMENT_H_INCLUDED
#define OSNMA_MANAGEMENT_H_INCLUDED

#include <stdio.h>
#include "osnmaDataStructures.h"
#include "ngeneConfiguration.h"
#include "cryptoFunctions.h"
#include "ngeneDataStructure.h"
#include "channelDataStructure.h"



void InitOsnmaParameters(TNGeneConfiguration *cfg,TOsnmaParameters *osnma);
void CopyOsnmaParameters(TOsnmaParameters *src,TOsnmaParameters *dst,int overwriteKRootTime);
void GetOsnmaParameters(TOsnmaParameters *osnma,unsigned char *chDsm,int overwrite,FILE *filePtr);
void InitOsnmaData(TNGeneConfiguration *cfg,TOsnmaData *osnma);
void RestartTeslaKeyVericationVariables(TOsnmaData *osnma);
void DeinitOsnmaData(TOsnmaData *osnma);


void InitChannelOsnmaVariables(TChannel *ch, TChannelOsnmaVariables *osnma);

void UpdateChannelOsnmaVariables(TOsnmaData *rxOsnma,TOsnmaAuthentication *auth,TChannelOsnmaVariables *chOsnma,int wn,int tow,int wordType,unsigned char *evenPart,unsigned char *oddPart,unsigned char *rsvd1);


void RestartChannelOsnmaVariables(TOsnmaData *rxOsnma, TChannelOsnmaVariables *chOsnma);

int CheckTeslaValidationThread(TOsnmaData *rxOsnma);

void SaveMackInMackList(TOsnmaData *rxOsnma,TChannelOsnmaVariables *chOsnma,int wn,int tow);

void SaveNavdataInMackList(TOsnmaData *rxOsnma, TChannelOsnmaVariables *chOsnma,int wn,int tow);

#ifndef PATCH_IGNORE_IOD_IN_MAC_AUTHENTICATION
int ExtractAndCheckBitsForMacVerification(TOsnmaData *rxOsnma,TSatConstellation satType,TSatSignal satSignal,int prn,int chId,int wn,int tow,TAdkd adkd,unsigned int iodMac,TNavDataList **navDataCurrPtr,TNavDataList **navDataPrevPtr);
int ExtractBitsForMacVerification(TOsnmaData *rxOsnma,TSatConstellation satType,TSatSignal satSignal,int prn,int chId,int wn,int tow,TAdkd adkd,unsigned int iodMac,TNavDataList **navDataCurrPtr,TNavDataList **navDataPrevPtr);
#else
int ExtractAndCheckBitsForMacVerification(TOsnmaData *rxOsnma,TSatConstellation satType,TSatSignal satSignal,int prn,int chId,int wn,int tow,TAdkd adkd,unsigned int *iodMac,TNavDataList **navDataCurrPtr,TNavDataList **navDataPrevPtr);
int ExtractBitsForMacVerification(TOsnmaData *rxOsnma,TSatConstellation satType,TSatSignal satSignal,int prn,int chId,int wn,int tow,TAdkd adkd,TNavDataList **navDataCurrPtr,TNavDataList **navDataPrevPtr);
#endif

int ExtractNavDataBits(TSatConstellation satType,TAdkd adkd,unsigned char *navData1,int *wordTypes1,unsigned char *navData2,int *wordTypes2,unsigned char *arrayOut,int *len);

void SaveDsmBlock(TOsnmaData *rxOsnma, TChannelOsnmaVariables *chOsnma,int wn,int tow);

void DeleteChannelFromOsnmaData(TOsnmaData *rxOsnma,int chId);

#endif /* OSNMA_MANAGEMENT_H_INCLUDED */
