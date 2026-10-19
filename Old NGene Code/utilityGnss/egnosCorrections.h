/**
 * @file egnosCorrections.h
 *
 * @author Mario Nicola
 *
 * @date 06/08/2015
 *
 * @version 1.0.0
 *
 * @brief Functions used to manage the EGNOS corrections.
 */

#ifndef EGNOS_CORRECTIONS_H_INCLUDED
#define EGNOS_CORRECTIONS_H_INCLUDED

#include "egnosCorrectionsDataStructure.h"
#include "channelDataStructure.h"

int InitEgnosCorrections(TNGeneConfiguration *cfg,TEgnosCorrections *egnosCorr);
void DeinitEgnosCorrections(TEgnosCorrections *egnosCorr);

void CleanEgnosCorrections(TEgnosCorrections *egnosCorr);
void CleanEgnosIonoCorrections(TEgnosCorrections *egnosCorr,int bandNumber);

void CompleteUdreiInformation(unsigned int udrei,double *udreiMeters,double *sigma2Udre,unsigned char *monitored,unsigned char *toBeUsed);
void CompleteFastDegradationFactorInformation(TEgnosFastCorrectionDegradationFactor *fastDegFact);
void CompleteGiveInformation(TEgnosIonoIgpDelayCorrections *ionoCorr);

void SetEgnosPrnMaskIndex(int nChannels,TChannel *chArray,TEgnosCorrections *egnosCorr);
double GetEgnosFastCorrection(TNGeneConfiguration *cfg,int maskIndex,TEgnosCorrections *egnosCorr,double rxTime);
double GetEgnosLongTermClockCorrection(TNGeneConfiguration *cfg,int iodc,int maskIndex,TEgnosCorrections *egnosCorr,double txTime);
void GetEgnosLongTermPositionCorrection(TNGeneConfiguration *cfg,int iode,int maskIndex,TEgnosCorrections *egnosCorr,double txTime,double *dx,double *dy,double *dz);


#endif /* EGNOS_CORRECTIONS_H_INCLUDED */
