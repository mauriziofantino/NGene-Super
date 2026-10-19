/**
 * @file mackList.h
 *
 * @author Mario Nicola
 *
 * @date 17/04/2019
 *
 * @version 1.0.0
 *
 * @brief Functions used to create and manage a list with all the MACK information
 */
#ifndef MACK_LIST_H_INCLUDED
#define MACK_LIST_H_INCLUDED

#include "osnmaDataStructures.h"

TMackList * FindInMackList(TMackList *head,TSatConstellation satType,TSatSignal satSignal,int satId,int chId);
TMackList * PushInMackList(TMackList *head,TSatConstellation satType,TSatSignal satSignal,int satId,int chId);

void DeleteMackList(TMackList **head);
void DeleteAllKeysAndMacsFromMackList(TMackList *head);

TMacList * PushNewMac(TOsnmaParameters *osnmaPars,TMackList * mackList);
void DeleteAMacFromMacList(TMackList * mackList,TMacList *macPtr);
void DeleteMacsOlderThan(TMackList * mackPtr,int wn,int tow);
void DeleteMacIfKeyOlderThan(TMackList * mackPtr,TMacList *macPtr, int wn,int tow);

void DeleteMacList(TMackList * mackList);

TKeyList * PushNewKey(TOsnmaParameters *osnmaPars,TMackList * mackList);
void DeleteAKeyFromKeyList(TMackList * mackList,TKeyList *keyPtr);
void DeleteKeysOlderThan(TMackList * mackPtr,int wn,int tow);
void DeleteKeyList(TMackList * mackList);

TNavDataList * PushNewNavData(TMackList * mackList);
TNavDataList * FindNavData(TNavDataList *navData,int wn,int tow);
void DeleteANavDataFromNavDataList(TMackList * mackList,TNavDataList *navPtr);
void DeleteNavDataOlderThan(TMackList * mackPtr,int wn,int tow);
void DeleteNavDataList(TMackList * mackList);
int GetOldestTowInNavDataList(TMackList *head,TSatConstellation satType,TSatSignal satSignal,int satId,int chId,int *wn,int *tow);

TMackList * DeleteChannelFromMackList(TMackList *head,int chId);

#endif /* MACK_LIST_H_INCLUDED */
