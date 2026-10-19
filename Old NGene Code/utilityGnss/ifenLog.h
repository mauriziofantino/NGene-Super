/**
 * @file ifenLog.h
 *
 * @author Mario Nicola
 *
 * @date 28/10/2020
 *
 * @version 1.0.0
 *
 * @brief Functions used to save the navigation message symbols/bits in the IFEN log format
 */

#ifndef IFENLOG_H_INCLUDED
#define IFENLOG_H_INCLUDED


/**
 * @brief Perform the log of symbols in the Galileo I/NAV navigation message in IFEN format
 *
 * @param fp points to the output file
 * @param logTime points to the time of the first entry in the file (WN+TOW in seconds)
 * @param wn is the transmission time of the begin of the current page part (WN)
 * @param tow is the transmission time of the begin of the current page part (TOW)
 * @param prn is the current PRN
 * @param symbols is the array containing all the symbols in the current page part (from the parser)
 */
void SaveGalileoINavSymbolsInIfenLog(FILE *fp,long long int *logTime,int wn,int tow,int prn,unsigned char *symbols);


/**
 * @brief Perform the log of symbols in the Galileo F/NAV navigation message in IFEN format
 *
 * @param fp points to the output file
 * @param logTime points to the time of the first entry in the file (WN+TOW in seconds)
 * @param wn is the transmission time of the begin of the current page (WN)
 * @param tow is the transmission time of the begin of the current page (TOW)
 * @param prn is the current PRN
 * @param symbols is the array containing all the symbols in the current page (from the parser)
 */
void SaveGalileoFNavSymbolsInIfenLog(FILE *fp,long long int *logTime,int wn,int tow,int prn,unsigned char *symbols);


/**
 * @brief Perform the log of bits in the GPS navigation message in IFEN format
 *
 * @param fp points to the output file
 * @param logTime points to the time of the first entry in the file (WN+TOW in seconds)
 * @param wn is the transmission time of the begin of the current subframe (WN)
 * @param tow is the transmission time of the begin of the current subframe (TOW)
 * @param prn is the current PRN
 * @param bits is the array containing all the bits in the current subframe (from the parser)
 */
void SaveGpsL1CaBitsInIfenLog(FILE *fp,long long int *logTime,int wn,int tow,int prn,unsigned char *bits);

#endif /* IFENLOG_H_INCLUDED */
