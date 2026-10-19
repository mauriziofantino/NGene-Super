/**
 * @file egnosMessageParser.h
 *
 * @author Mario Nicola
 *
 * @date 21/04/2015
 *
 * @version 1.0.0
 *
 * @brief Functions used to parse the EGNOS navigation message.
 */

#ifndef EGNOS_MESSAGE_PARSER_H_INCLUDED
#define EGNOS_MESSAGE_PARSER_H_INCLUDED


int InitEgnosParser(TNGeneConfiguration *cfg,TChannel *ch);

void DeinitEgnosParser(TChannel *ch);

void StartEgnosParser(TNGeneConfiguration *cfg,TChannel *ch);

void ParseEgnosMessage(TNGeneConfiguration *cfg,TNGeneData *rx,TChannel *ch,float corr,double time);

#endif /* EGNOS_MESSAGE_PARSER_H_INCLUDED */
