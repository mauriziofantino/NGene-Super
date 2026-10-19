/**
 * @file ifenLog.c
 *
 * @author Mario Nicola
 *
 * @date 28/10/2020
 *
 * @version 1.0.0
 *
 * @brief Functions used to save the navigation message symbols/bits in the IFEN log format
 */


#include <stdio.h>
#include "navigationMessageParser.h"
#include "timeConstants.h"


#define BYTES_PER_WORD  4


void SaveGalileoINavSymbolsInIfenLog(FILE *fp,long long int *logTime,int wn,int tow,int prn,unsigned char *symbols)
{
	long long int t;
	unsigned char byte;
	int idx;

	/* check whether the log is disabled */
	if(fp==NULL)
	{
		return;
	}

	/* check time (must be valid) */
	if((wn<0) || (tow<0))
	{
		return;
	}
	t = ((long long int)wn*SECONDS_PER_WEEK) + (long long int)tow;

	/* if this is the first saved page part */
	if((*logTime)<0)
	{
		// write in the file (complete the header!)
		fprintf(fp,"# START_TIME_TAG = %lld\n",t);
		// update logTime
		(*logTime) = t;
		t = 0;
	}
	else
	{
		// relative time
		t -= (*logTime);
	}

	/* save the page part */
	// time
	fprintf(fp,"%lld,%d,",t,prn);
	// the preamble is not contained in the symbols array
	// preamble, write the 8 MS bits
	fprintf(fp,"%02x",(unsigned char)(GAL_PREAMBLE_BITS>>2));
	// preamble, save the 2 LS bits
	byte = (unsigned char)(GAL_PREAMBLE_BITS & 0x03)<<6;
	// write all the symbols in the symbols array
	for(idx=0;idx<GAL_ENCODED_PAGE_PART_LENGTH_BYTES;idx++)
	{
		byte = byte | ((symbols[idx]>>2) & 0x3f);
		fprintf(fp,"%02x",byte);
		byte = (unsigned char)(symbols[idx] & 0x03)<<6;
	}
	// print last two bits (padded with zeros on the right)
	fprintf(fp,"%02x",byte);
	// terminates the row
	fprintf(fp,"\n");
}


void SaveGalileoFNavSymbolsInIfenLog(FILE *fp,long long int *logTime,int wn,int tow,int prn,unsigned char *symbols)
{
	long long int t;
	unsigned char byte;
	int idx;

	/* check whether the log is disabled */
	if(fp==NULL)
	{
		return;
	}

	/* check time (must be valid) */
	if((wn<0) || (tow<0))
	{
		return;
	}
	t = ((long long int)wn*SECONDS_PER_WEEK) + (long long int)tow;

	/* if this is the first saved page part */
	if((*logTime)<0)
	{
		// write in the file (complete the header!)
		fprintf(fp,"# START_TIME_TAG = %lld\n",t);
		// update logTime
		(*logTime) = t;
		t = 0;
	}
	else
	{
		// relative time
		t -= (*logTime);
	}

	/* save the page */
	// time
	fprintf(fp,"%lld,%d,",t,prn);
	// the preamble is not contained in the symbols array
	// preamble, write the 8 MS bits
	fprintf(fp,"%02x",(unsigned char)(FNAV_PREAMBLE_BITS>>4));
	// preamble, save the 4 LS bits
	byte = (unsigned char)(FNAV_PREAMBLE_BITS & 0x0f)<<4;
	// write all the symbols in the symbols array
	for(idx=0;idx<FNAV_ENCODED_PAGE_LENGTH_BYTES;idx++)
	{
		byte = byte | ((symbols[idx]>>4) & 0x0f);
		fprintf(fp,"%02x",byte);
		byte = (unsigned char)(symbols[idx] & 0x0f)<<4;
	}
	// print last four bits (padded with zeros on the right)
	fprintf(fp,"%02x",byte);
	// terminates the row
	fprintf(fp,"\n");
}


void SaveGpsL1CaBitsInIfenLog(FILE *fp,long long int *logTime,int wn,int tow,int prn,unsigned char *bits)
{
	long long int t;
	unsigned char byte;
	int inputIdx = 0;
	int wordIdx;
	int bitIdx;
	unsigned char inputMask;
	unsigned char outputMask;

	/* check whether the log is disabled */
	if(fp==NULL)
	{
		return;
	}

	/* check time (must be valid) */
	if((wn<0) || (tow<0))
	{
		return;
	}
	t = ((long long int)wn*SECONDS_PER_WEEK) + (long long int)tow;

	/* if this is the first saved page part */
	if((*logTime)<0)
	{
		// write in the file (complete the header!)
		fprintf(fp,"# START_TIME_TAG = %lld\n",t);
		// update logTime
		(*logTime) = t;
		t = 0;
	}
	else
	{
		// relative time
		t -= (*logTime);
	}

	/* save the subframe */
	// time
	fprintf(fp,"%lld,%d,",t,prn);
	// save the bits
	inputIdx = 0;
	outputMask = 0x80;
	byte = 0x00;
	for(wordIdx=0;wordIdx<GPS_SUBFRAME_LENGTH;wordIdx++)
	{
		// skip 2 MS bits of the word (repeated CRC bits from the previous word)
		inputMask = 0x20;
		// elaborate each bit in a word
		for(bitIdx=0;bitIdx<GPS_WORD_LENGTH;bitIdx++)
		{
			byte = ((bits[inputIdx] & inputMask)>0) ? (byte | outputMask) : byte;
			// update output mask, save byte if complete
			outputMask = outputMask >> 1;
			if(outputMask==0x00)
			{
				fprintf(fp,"%02x",byte);
				byte = 0x00;
				outputMask = 0x80;
			}
			// update input mask, change byte if all bits have been saved
			inputMask = inputMask >> 1;
			if(inputMask==0x00)
			{
				inputMask = 0x80;
				inputIdx++;
			}
		}
	}
	// print last four bits (padded with zeros on the right)
	fprintf(fp,"%02x",byte);
	// terminates the row
	fprintf(fp,"\n");
}

