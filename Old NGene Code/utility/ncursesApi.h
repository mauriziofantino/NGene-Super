/**
 * @file ncursesApi.h
 *
 * @author Mario Nicola
 *
 * @date 25/11/2014
 *
 * @version 1.0.0
 *
 * @brief Output to the NCurses interface
 */

#ifndef UTILITY_NCURSES_API_H_INCLUDED
#define UTILITY_NCURSES_API_H_INCLUDED


#include <ncurses.h>
#include "ngeneConfiguration.h"
#include "ngeneDataStructure.h"
#include "ncursesDataStructure.h"


/**
 * @brief InitNCurses initializes the NCurse user interface
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 * @param firstExecution is TRUE_FLAG when the InitNCurses is executed for the first time (during receiver initialization)
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int InitNCurses(TNGeneConfiguration *cfg,TNGeneData *rx,int firstExecution);


/**
 * @brief Deinits the NCurses user interface
 *
 * @param nc points to the structure storing the NCurses support variables
 */
void DeinitNCurses(TNCursesData *nc);


/**
 * @brief Plots a text string in the NCurser user interface
 *
 * @param nc points to the structure storing the NCurses support variables
 * @param logString is the text string that willbe printed
 * @param severity is an enumerate that specifies the severity of the message
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int NcDrawRxLog(TNCursesData *nc,char *logString, int severity);


/**
 * @brief Plots the results of the receiver in the NCurses user interface
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 */
void NcCompleteLog(TNGeneConfiguration *cfg,TNGeneData *rx);

#endif /* UTILITY_NCURSES_API_H_INCLUDED */
