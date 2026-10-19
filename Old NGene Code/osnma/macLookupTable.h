/**
 * @file macLookupTable.h
 *
 * @author Mario Nicola
 *
 * @date 08/07/2019
 *
 * @version 1.0.0
 *
 * @brief Functions used to manage the MAC Lookup Table
 */

#ifndef MAC_LOOKUP_TABLE_H_INCLUDED
#define MAC_LOOKUP_TABLE_H_INCLUDED

#include "osnmaDataStructures.h"


void InitMacLut(TOsnmaParameters *pars);

void DeinitMacLut(TOsnmaParameters *pars);

void LoadMacSequence(TOsnmaParameters *pars);

#endif
