/**
 * @file channelManager.h
 *
 * @author Mario Nicola
 *
 * @date 23/08/2014
 *
 * @version 1.0.0
 *
 * @brief Functions used to manage the channel state machine
 */
#ifndef CHANNEL_MANAGER_H_INCLUDED
#define CHANNEL_MANAGER_H_INCLUDED

#include "ngeneConfiguration.h"
#include "channelDataStructure.h"



/**
 * @brief Initializes the structure used to manage the scheduling of the channels
 *        (i.e. allocates PRNs to the channels and chooses which channels can go in acquisition)
 *
 * @param cfg points to the receiver configuration
 * @param chSch points to the scheduling structure
 * @param numOfRealGalileoPrns is the number of Galileo PRNs reported in the Galileo_real_PRNs.txt configuration file
 * @param realGalileoPrns points to the list of real Galileo PRNs
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int InitChannelScheduler(TNGeneConfiguration *cfg,TChannelScheduler *chSch,int numOfRealGalileoPrns,int *realGalileoPrns);

/**
 * @brief De-initializes the structure used to manage the scheduling of the channels
 *
 * @param  chSch points to the scheduling structure
 */
void DeinitChannelScheduler(TChannelScheduler *chSch);

/**
 * @brief Initializes the structure used to manage a channel
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 * @param ch points to the channel to be initialized
 */
void InitChannel(TNGeneConfiguration *cfg,TNGeneData *rx,TChannel *ch);

/**
 * @brief Deinitializes the structure used to manage a channel
 *
 * @param ch points to the channel to be deinitialized
 */
void DenitChannel(TChannel *ch);

/**
 * @brief Analyzes how many samples are required to continue elaboration
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 *
 * @return The maximum number of required samples
 */
unsigned int GetMaximumNumberOfSamplesNeeded(TNGeneConfiguration *cfg,TNGeneData *rx);

/**
 * @brief Analyzes how many samples have been elaborated by all the channels
 *        and then can be discarded from the input buffer
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 *
 * @return Number of samples that can be discarded
 */
unsigned int GetNumberOfUsedSamples(TNGeneConfiguration *cfg,TNGeneData *rx);

/**
 * @brief Updates the buffer offset in each channel taking into account the number of samples
 *        discarded from the input buffer
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 * @param deletedSamples number of samples deleted from the input buffer
 */
void CleanBufferOffsetForAllChannels(TNGeneConfiguration *cfg,TNGeneData *rx,unsigned int deletedSamples);


/**
 * @brief Finds the pointer to the first samples to be elaborated by a given channel.
 *        Takes into account the channel offset
 *
 * @param ch points to the current channel
 * @param buf points to the circular buffer structure
 * @param firstBunch points to the address of the first sample
 * @param firstBunchSize points to the number of samples starting from *firstBunch
 * @param secondBunch points to the address of the first sample in the second bunch of samples (if any)
 * @param secondBunchSize points to the number of samples starting from *secondBunch (if any)
 */
void GetSamplePointersForChannel(TChannel *ch,TCircularBuffer *buf,char **firstBunch, unsigned int *firstBunchSize,char **secondBunch, unsigned int *secondBunchSize);


/**
 * @brief This function verifies if all the channels are in the UNUSED_STATE.
 *        Used to debug purpose in order to stop the receiver when no more useful elaboration is performed.
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 *
 * @return TRUE_FLAG if all channels are in UNUSED_STATE, FALSE_FLAG elsewhere
 */
int AllChannelsUnused(TNGeneConfiguration *cfg,TNGeneData *rx);


/**
 * @brief Implements the channel state machine (for the current channel) and calls functions performing the signal elaboration!
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 * @param ch points to the current channel
 * @param firstBunch is the address of the first sample to be elaborated
 * @param firstBunchSize is the number of samples starting from firstBunch
 * @param secondBunch is the address of the first sample in the second bunch of samples (if any)
 * @param secondBunchSize is the number of samples starting from secondBunch (if any)
 */
void DoChannelManager(TNGeneConfiguration *cfg,TNGeneData *rx,TChannel *ch,char *firstBunch, unsigned int firstBunchSize,char *secondBunch, unsigned int secondBunchSize);

int FindChId(TNGeneConfiguration *cfg,TNGeneData *rx, TSatConstellation satType, int satId);

void ReleaseTracking(TNGeneConfiguration *cfg,TChannelScheduler *chSch,TChannel *ch);

void HardRestartChannels(TNGeneConfiguration *cfg,TNGeneData *rx);

#endif /* CHANNEL_MANAGER_H_INCLUDED */
