/**
 * @file macLookupTable.c
 *
 * @author Mario Nicola
 *
 * @date 08/07/2019
 *
 * @version 1.0.0
 *
 * @brief Functions used to manage the MAC Lookup Table
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "osnmaDataStructures.h"
#include "ioFunctions.h"

void InitMacLut(TOsnmaParameters *pars)
{
	TMacLut *lut = &(pars->macLut);

	lut->lutValid = FALSE_FLAG;
	lut->id = -1;
	lut->sct = -1;
	lut->nmack = -1;
	lut->macsPerBlock = -1;
	lut->sequenceLen = 0;
	lut->sequences = NULL;
}


void DeinitMacLut(TOsnmaParameters *pars)
{
	TMacLut *lut = &(pars->macLut);
	int idx;
	TMacLutSequence *seq;

	/* free memory */
	if(lut->sequences!=NULL)
	{
		for(idx=0;idx<lut->sct;idx++)
		{
			seq = &(lut->sequences[idx]);
			if(seq->adkdSequence!=NULL)
			{
				free(seq->adkdSequence);
			}
			if(seq->sequence!=NULL)
			{
				free(seq->sequence);
			}
		}
		free(lut->sequences);
		lut->sequences = NULL;
	}
	/* other variables */
	lut->lutValid = FALSE_FLAG;
	lut->id = -1;
	lut->sct = -1;
	lut->nmack = -1;
	lut->macsPerBlock = -1;
	lut->sequenceLen = 0;
}


void LoadMacSequence(TOsnmaParameters *pars)
{
	TMacLut *lut = &(pars->macLut);
	FILE *lutFile;
	char row[MAX_ROW_LENGTH];
	int rowLen;
	int rowIdx;
	char **sequencesComplete;
	int numElements;
	const int elementSize = 3;
	const char flexString[] = "FLX";
	int elemIdx;
	int charIdx;
	int sctIdx;
	TMacLutSequence *seq;
	int elemOffset;


	/* Deinit the LUT structure */
	DeinitMacLut(pars);

	/* if the index of the row to be read is not valid,
	 * it is impossible to continue */
	if(pars->parametersValid==FALSE_FLAG)
	{
		return;
	}
	/* open the file */
	lutFile = fopen(pars->macLutFilename,"r");
	if(lutFile==NULL)
	{
		SwrxLog(NGENE_LOG_SEVERITY_WARNING,"Cannot find the MAC LUT file %s. MACSEQs will not be verified!",pars->macLutFilename);
		return;
	}
	/* parse the file */
	rowIdx = -1;
	while((feof(lutFile)==0) && (rowIdx!=pars->macLutIndex))
	{
		if(fgets(row,MAX_ROW_LENGTH,lutFile)!=NULL)
		{
			if(row[0]!='#')
			{
				sscanf(row,"%d",&rowIdx);
			}
		}
	}
	/* index not found, return */
	if(rowIdx!=pars->macLutIndex)
	{
		SwrxLog(NGENE_LOG_SEVERITY_WARNING,"Cannot find MAC LUT row %d in %s. MACSEQs will not be verified!",pars->macLutIndex,pars->macLutFilename);
		return;
	}
	rowLen = strlen(row);
	/* read parameters */
	sscanf(row,"%d %d %d %d",&(lut->id),&(lut->sct),&(lut->nmack),&(lut->macsPerBlock));
	/* check parameters consistency */
	if((lut->nmack!=pars->nmack) || (lut->macsPerBlock<pars->macPerMackBlock))
	{
		SwrxLog(NGENE_LOG_SEVERITY_WARNING,"OSNMA parameters not coherent with MAC LUT row %d in %s. MACSEQs will not be verified!",pars->macLutIndex,pars->macLutFilename);
		return;
	}
	lut->sequenceLen = lut->nmack*lut->macsPerBlock;
	numElements = lut->sct*lut->sequenceLen;
	/* load all the elements of the sequences */
	sequencesComplete = (char **)malloc(numElements*sizeof(char *));
	for(elemIdx=0;elemIdx<numElements;elemIdx++)
	{
		sequencesComplete[elemIdx] = (char *)malloc(elementSize*sizeof(char));
	}
	charIdx = 0;
	while(row[charIdx]!=',')
	{
		charIdx++;
	}
	charIdx -= elementSize;
	elemIdx = 0;
	while((elemIdx<numElements) && (charIdx<rowLen))
	{
		memcpy(sequencesComplete[elemIdx],&(row[charIdx]),elementSize);
		elemIdx++;
		charIdx += (elementSize + 1); // discard also the comma
	}
	/* continue only if all elements have been read */
	if(elemIdx==numElements)
	{
		lut->sequences = (TMacLutSequence *)malloc(lut->sct*sizeof(TMacLutSequence));
		elemOffset = 0;
		for(sctIdx=0;sctIdx<lut->sct;sctIdx++)
		{
			/* allocate each sequence, read them and count FLEXs */
			seq = &(lut->sequences[sctIdx]);
			seq->adkdSequence = (TAdkd *)malloc(lut->sequenceLen*sizeof(TAdkd));
			seq->sequence = (TMacLutElement *)malloc(lut->sequenceLen*sizeof(TMacLutElement));
			seq->flexLen = 0;
			for(elemIdx=0;elemIdx<lut->sequenceLen;elemIdx++)
			{
				if(strncmp(sequencesComplete[elemIdx+elemOffset],flexString, elementSize)==0)
				{
					seq->sequence[elemIdx] = MAC_LUT_FLEX;
					seq->flexLen++;
				}
				else
				{
					seq->adkdSequence[elemIdx] = 10*((int)(sequencesComplete[elemIdx+elemOffset][0]-'0')) + ((int)(sequencesComplete[elemIdx+elemOffset][1]-'0'));
					switch(sequencesComplete[elemIdx+elemOffset][2])
					{
					case 'S':
						seq->sequence[elemIdx] = MAC_LUT_SELF;
						break;
					case 'E':
						seq->sequence[elemIdx] = MAC_LUT_CROSS_GAL;
						break;
					case 'G':
						seq->sequence[elemIdx] = MAC_LUT_CROSS_GPS;
						break;
					default:
						seq->sequence[elemIdx] = MAC_LUT_INVALID;
						break;
					}
				}
			}
			elemOffset += lut->sequenceLen;
		}
	}
	/* now the LUT is valid */
	lut->lutValid = TRUE_FLAG;
	/* deinit */
	for(elemIdx=0;elemIdx<numElements;elemIdx++)
	{
		free(sequencesComplete[elemIdx]);
	}
	free(sequencesComplete);
	fclose(lutFile);
}
