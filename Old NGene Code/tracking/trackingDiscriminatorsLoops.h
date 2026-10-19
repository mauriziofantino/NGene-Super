/**
 * @file trackingDiscriminatorsLoops.h
 *
 * @author Mario Nicola
 *
 * @date 04/09/2014
 *
 * @version 1.0.0
 *
 * @brief Functions used to perform tracking discriminators and loop filters.
 */

#ifndef TRACKING_DISCRIMINATORS_LOOPS_H_INCLUDED
#define TRACKING_DISCRIMINATORS_LOOPS_H_INCLUDED

#include "ngeneConfiguration.h"
#include "channelDataStructure.h"
#include "trackingDataStructure.h"
#include "ngeneDataStructure.h"

/**
 * @brief Calculates the coefficients of the loop filters (FLL,PLL,DLL).
 * 		  Uses Borre-Akos model (already in NGene)
 *
 * @param cfg points to the receiver configuration
 * @param ch points to the current channel
 * @param trk points to the tracking structure
 */
void CalculateLoopCoefficients(TNGeneConfiguration *cfg,TChannel *ch,TTracking *trk);

/**
 * @brief Re-initializes variables in the FLL and PLL loops
 *
 * @param ch points to the current channel
 * @param trk points to the tracking structure
 */
void RestartCarrierLoop(TChannel *ch,TTracking *trk);

/**
 * @brief Re-initializes variable in the DLL loop
 *
 * @param ch points to the current channel
 * @param trk points to the tracking structure
 */
void RestartCodeLoop(TChannel *ch,TTracking *trk);

/**
 * @brief Initializes all discriminators and loop variables
 *
 * @param cfg points to the receiver configuration
 * @param ch points to the current channel
 * @param trk points to the tracking structure
 */
void InitTrackingLoops(TNGeneConfiguration *cfg,TChannel *ch,TTracking *trk);

/**
 * @brief Verifies the lock of the FLL. If the FLL is locked, PLL is used.
 *  	  Elsewhere, FLL is selected.
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 * @param ch points to the current channel
 * @param trk points to the tracking structure
 */
TCarrierLoopChoice ChooseCarrierLoop(TNGeneConfiguration *cfg,TNGeneData *rx,TChannel *ch,TTracking *trk);

/**
 * @brief Evaluates the FLL discriminator
 *
 * @param trk points to the tracking structure
 */
void FllDiscriminator(TTracking *trk);

/**
 * @brief Evaluates the PLL discriminator
 *
 * @param trk points to the tracking structure
 */
void PllDiscriminator(TTracking *trk);

/**
 * @brief Evaluates the DLL discriminator
 *
 * @param trk points to the tracking structure
 */
void DllDiscriminator(TTracking *trk);

/**
 * @brief Filters carrier (FLL or PLL) discriminator. Corrects estimated carrier frequency.
 *
 * @param cfg points to the receiver configuration
 * @param ch points to the current channel
 * @param trk points to the tracking structure
 */
void CarrierLoop(TNGeneConfiguration *cfg,TChannel *ch, TTracking *trk);

/**
 * @brief Filters code discriminator. Corrects estimated code rate.
 *
 * @param ch points to the current channel
 * @param trk points to the tracking structure
 */
void CodeLoop(TChannel *ch, TTracking *trk);


#endif /* TRACKING_DISCRIMINATORS_LOOPS_H_INCLUDED */
