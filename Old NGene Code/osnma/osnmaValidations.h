/**
 * @file osnmaValidations.h
 *
 * @author Mario Nicola
 *
 * @date 03/11/2020
 *
 * @version 1.0.0
 *
 * @brief Functions for the management of the MAC validations
 */

#ifndef OSNMA_VALIDATIONS_H_INCLUDED
#define OSNMA_VALIDATIONS_H_INCLUDED


#include "ngeneConfiguration.h"
#include "osnmaDataStructures.h"
#include "osnmaValidationsDataStructures.h"
#include "ngeneDataStructure.h"



void InsertMacValidation(TOsnmaAuthentication *auth,int chIdxA,int chIdxN,int prnN,int wn,int tow,TAdkd adkd,unsigned int iod,TValidationStatus validityFlag);
TMacValidations * ExtractMacValidation(TOsnmaAuthentication *auth,int chIdxA,int chIdxN);
void DeleteMacValidationsOneList(TOsnmaAuthentication *auth,int chIdxA,int chIdxN);
void DeleteMacValidationsAuthenticatedChannel(TOsnmaAuthentication *auth,int chIdxN);
void DeleteMacValidationsAuthenticatingChannel(TOsnmaAuthentication *auth,int chIdxA);

void InitOsnmaAuthenticationStruct(TNGeneConfiguration *cfg,TOsnmaAuthentication *auth);
void ResetOsnmaAuthenticationStruct(TOsnmaAuthentication *auth);
void ResetChannelAuthenticationStruct(TOsnmaAuthentication *auth,int chIndex);
void DeinitOsnmaAuthenticationStruct(TOsnmaAuthentication *auth);
void CheckOsnmaDsmTeslaMacseq(TNGeneConfiguration *cfg,TNGeneData *rx);
void ManageOsnmaAuthentication(unsigned long long int rxCounter,TOsnmaData *osnma,TOsnmaAuthentication *auth);
void ManageOsnmaTimeout(TNGeneConfiguration *cfg,TNGeneData *rx);
void SaveAuthenticationLog(int wn,double tow,TOsnmaData *osnma,TOsnmaAuthentication *auth);

void SaveOsnmaSimpleLog(TNGeneConfiguration *cfg,TNGeneData *rx);

#endif /* OSNMA_VALIDATIONS_H_INCLUDED */
