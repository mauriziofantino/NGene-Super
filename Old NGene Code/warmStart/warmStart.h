/**
 * @file warmStart.h
 *
 * @author Mario Nicola
 *
 * @date 13/04/2021
 *
 * @version 1.0.0
 *
 * @brief Functions that manage the Warm Start.
 */

#ifndef WARM_START_H_INCLUDED
#define WARM_START_H_INCLUDED

#include "warmStartConfiguration.h"
#include "ngeneConfiguration.h"
#include "ngeneDataStructure.h"

/**
 * @brief Initialization of the variables used to manage the Warm Start.
 *        It parses the almanac files.
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int InitWarmStartData(TNGeneConfiguration *cfg,TNGeneData *rx);

/**
 * @brief Deinit the variables used to manage the Warm Start.
 *
 * @param warmData points to the Warm Start data structure
 */
void DeinitWarmStartData(TWarmStartData *warmData);

/**
 * @brief Updates the visibility list.
 *
 * @details Checks if the time of the visibility update has come.
 *          Evaluates the position, velocity, azimuth and elevation of the satellites.
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver
 */
void UpdateVisibilityList(TNGeneConfiguration *cfg,TNGeneData *rx);

/**
 * @brief Check the visibility of a satellite.
 *
 * @details Used by GetNextPrnForAcquisition to verify whether it can allocate the PRN or not.
 *
 * @param warmCfg points to the warm start configuration
 * @param warmData points to the warm start data structure
 * @param satType current constellation
 * @satId satId current PRN
 *
 * @return TRUE_FLAG when the current PRN can be allocated by GetNextPrnForAcquisition, FALSE_FLAG if it should be discarded
 */
int CheckSatVisibility(TWarmStartConfiguration *warmCfg,TWarmStartData *warmData,TSatConstellation satType,int satId);

/**
 * @brief Mark the satellite as available in the visibility list.
 *
 * @param warmCfg points to the warm start configuration
 * @param warmData points to the warm start data structure
 * @param satType current constellation
 * @satId satId current PRN
 */
void FreeInTheVisibilityList(TWarmStartConfiguration *warmCfg,TWarmStartData *warmData,TSatConstellation satType,int satId);

#endif /* WARM_START_H_INCLUDED */
