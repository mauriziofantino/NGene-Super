/**
 * @file mainTestCryptoFunctions.c
 *
 * @author Micaela Troglia Gamba
 *
 * @date 31/05/2019
 *
 * @version 0.1
 *
 * @brief Testbed for openssl library function to be used for OSNMA.
 */

#include <openssl/sha.h>
#include <openssl/evp.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "base64.h"
#include <openssl/pem.h>
#include <openssl/x509.h>
#include "osnmaManagement.h"
#include "../../utilityGnss/navigationMessageParser.h"
#include "arrays.h"
#include "ioFunctions.h"
#include "chimeraCryptoFunctions.h"
#include "cryptoFunctions.h"


/* define global variables for the backup of xmm registers' values */
extern char *ngeneXmmsBackup;



int ReadAllPage(FILE *msg,unsigned char **tmpPageParts,char *tmpValid)
{
	int idx;
	char row[512];
	int time1;
	int prn;
	int pos;
	int numEntries = 0;
	fpos_t msgPos;
	int time2;

	for(idx=0;idx<32;idx++)
		tmpValid[idx] = 0;

	if(fgets(row,NUMBER_OF_BITS_IN_A_MESSAGE,msg)==NULL)
		return 0;

	sscanf(row,"%d,%d%n",&time1,&prn,&pos);
    ReadHexBytes(&(row[pos+1]),tmpPageParts[prn-1],NUMBER_OF_BYTES_IN_A_MESSAGE);
	tmpValid[prn-1] = 1;
	numEntries++;
    
    #ifdef DEBUG
    int i;
        for (i = 0; i < NUMBER_OF_BYTES_IN_A_MESSAGE; i++) 
        {
            printf("%02X", tmpPageParts[prn-1][i]);
        }
        printf("\n");
    #endif
	/* other entries with the same time */
	time2 = time1;
	while(time2==time1)
	{
		fgetpos(msg,&msgPos);
		if(fgets(row,NUMBER_OF_BITS_IN_A_MESSAGE,msg)==NULL)
			return numEntries;

		sscanf(row,"%d,%d%n",&time2,&prn,&pos);
		if(time2==time1)
		{
			ReadHexBytes(&(row[pos+1]),tmpPageParts[prn-1],NUMBER_OF_BYTES_IN_A_MESSAGE);
			tmpValid[prn-1] = 1;
			numEntries++;
            
            #ifdef DEBUG
            for (i = 0; i < NUMBER_OF_BYTES_IN_A_MESSAGE; i++) 
                {
                    printf("%02X", tmpPageParts[prn-1][i]);
                }
                printf("\n");
            #endif
		}
	}
	/* put back */
	fsetpos(msg,&msgPos);
	return numEntries;
}



int main(int argc, char* argv[])
{
    
    	/**********************************************************************/
    //char directory[] = "/media/micaela/DATA/FUNTIMES2/Datasets/DATI_CHIMERA/CHIE_20200609/CHIE/Output/Test01/";
    //char directory[] = "/media/micaela/DATA/FUNTIMES2/Datasets/DATI_CHIMERA/CHIE_20200609/CHIE/Output/Test02/";
    //char directory[] = "/media/micaela/DATA/FUNTIMES2/Datasets/DATI_CHIMERA/CHIE20200610/CHIE/Output/Test03/";
    //char directory[] = "/media/micaela/DATA/FUNTIMES2/Datasets/DATI_CHIMERA/CHIE_20200611/CHIE/Output/Test03/";
    //char directory[] = "/media/micaela/DATA/FUNTIMES2/Datasets/DATI_CHIMERA/CHIE_20200612/CHIE/Output/Test03/";
    //char directory[] = "/media/micaela/DATA/FUNTIMES2/Datasets/DATI_CHIMERA/CHIE_20200612c/CHIE/Output/Test03/";
    //char directory[] = "/media/micaela/DATA/FUNTIMES2/Datasets/DATI_CHIMERA/CHIE_20200619/CHIE/Output/Test04/";
    //char directory[] = "/media/micaela/DATA/FUNTIMES2/Datasets/DATI_CHIMERA/CHIE_20200623/CHIE/Output/Test05_df1/";

    char directory[2*MAX_ROW_LENGTH];
    
    
    directory[0] = '\0';
    strcat(directory, argv[1]);
    if(argc!=2)
    {
        printf("Error in passing the right number of parameters to the main!\n");
        return 1;
    }
    
    
    char dataFilename[] = "Ifen_GPS_NAV_L1C_bits.txt";
    char fullPathDataFilename[2*MAX_ROW_LENGTH];
    fullPathDataFilename[0] = '\0';
    strcat(fullPathDataFilename, directory);
    strcat(fullPathDataFilename, dataFilename);
    
    FILE *msg = fopen(fullPathDataFilename,"r");
	unsigned char *message[32];
	char tmpValid[32];
    unsigned char *chimeraEpoch[32];
	int finished = FALSE_FLAG;
	int prn;
    int byteIdxOut[32];
    int bitIdxOut[32];
    int pageIndex[32];
    //TChimeraConfiguration chimeraCfg;
    TChimeraData chimeraData;
    char flagDSVerify=-1;
    int toi=0;


    int pilotSymbolsToGenerate=1000;//2000;
    int numberOfPrns=32;

	/* prepare the XMM registers' backup */
	posix_memalign((void **)&(ngeneXmmsBackup),POSIX_ALIGNMENT,NUM_XMM_REGISTERS*POSIX_ALIGNMENT*sizeof(char));
	if(ngeneXmmsBackup==NULL)
	{
		fprintf(stderr,"Malloc returned a NULL pointer in main");
		exit(EXIT_FAILURE);
	}


	// feedback to the user
	if(msg==NULL)
	{
		//SwrxLog(NGENE_LOG_SEVERITY_ERROR,"NGene \"Fake Receiver\" cannot open input file. Exit...\n",dataFilename);
        printf("NGene \" Chimera Fake Receiver\" cannot open input file %s. Exit...\n",fullPathDataFilename);
        return FAILURE_RETURN;
	}
	//SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"Input from %s...",fullPathDataFilename);
    printf("Input from %s...\n",fullPathDataFilename);


	// init
	for(prn=0;prn<numberOfPrns;prn++)
	{
		message[prn] = (unsigned char *)calloc(NUMBER_OF_BYTES_IN_A_MESSAGE,sizeof(unsigned char));
        byteIdxOut[prn]=0;
        bitIdxOut[prn]=0;
        pageIndex[prn]=0;
	}
	for(prn=0;prn<numberOfPrns;prn++)
	{
		chimeraEpoch[prn] = (unsigned char *)calloc(NUMBER_OF_BYTES_IN_A_MESSAGE*NUMBER_OF_MESSAGE_IN_A_EPOCH,sizeof(unsigned char));
	}
    
	// discard header
	for(prn=0;prn<8;prn++)
	{
		fgets((char *) message[prn],NUMBER_OF_BYTES_IN_A_MESSAGE,msg);
	}
    
    
    //***********************
    InitChimeraParameters(&(chimeraData.chimeraPars),directory);
    if(InitChimeraData(&chimeraData)==FAILURE_RETURN)
    {
        return FAILURE_RETURN;
    }
    //*************************

	/* loop */
	while(finished==FALSE_FLAG)
	{
		if(ReadAllPage(msg,message,tmpValid)==0)
			finished = TRUE_FLAG;

		if(finished==FALSE_FLAG)
		{
            
            /*Collect one Chimera Epoch per prn*/
            for(prn=1;prn<=32;prn++)
			{
                if(tmpValid[prn-1]==TRUE_FLAG)
				{
                    CopyBitsUnaligned(0,0,message[prn-1] ,&byteIdxOut[prn-1],&bitIdxOut[prn-1],chimeraEpoch[prn-1],NUMBER_OF_BITS_IN_A_MESSAGE);
                    pageIndex[prn-1]++;

                    
                    if(pageIndex[prn-1]==NUMBER_OF_MESSAGE_IN_A_EPOCH)
                    {
                        /*Verify the DS*/
                        #ifdef CHIMERA_PROFILING
                        StartProfilingUpdate(&(chimeraData.dsProfiling));
                        #endif
                        flagDSVerify=Digital_SignatureVerification_fromEVPKEY(chimeraEpoch[prn-1],&chimeraData, &toi);
                        #ifdef CHIMERA_PROFILING
                        StopProfilingUpdate(&(chimeraData.dsProfiling),1);
                        #endif
                        if(flagDSVerify==SUCCESS_RETURN)
                        {
                            //SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"GPS PRN %d: Digital Signature successfully verified",prn);
                            printf("TOI = %d, GPS PRN %d: Digital Signature successfully verified\n",toi,prn);
                            #ifdef CHIMERA_PROFILING
                            StartProfilingUpdate(&(chimeraData.slowKeyProfiling));
                            #endif
                            SlowChannelMarkerKeyGeneration(chimeraData.DS,&chimeraData, prn);
                            #ifdef CHIMERA_PROFILING
                            StopProfilingUpdate(&(chimeraData.slowKeyProfiling),1);
                            #endif
                            
                            //**Load Fast Marker Key
                            LoadFastMarkerKey(&chimeraData);
                            chimeraData.indexFastKey=0;
                            //***
                            GeneratePatternList(&chimeraData);
                            //**SCA: SLOW Channel
                            //pilotSymbolsToGenerate=1000;
                            #ifdef CHIMERA_PROFILING
                            StartProfilingUpdate(&chimeraData.markerGenerationPerPilotProfiling);
                            #endif
                            MarkersGeneration(&chimeraData, SLOW_CHANNEL, prn,pilotSymbolsToGenerate);
                            #ifdef CHIMERA_PROFILING
                            StopProfilingUpdate(&chimeraData.markerGenerationPerPilotProfiling,pilotSymbolsToGenerate);
                            #endif
                            printf("TOI = %d, GPS PRN %d: %.1f seconds of Slow Markers generated\n",toi,prn,pilotSymbolsToGenerate*0.01);
                            //**SCA: FAST Channel
                            #ifdef CHIMERA_PROFILING
                            StartProfilingUpdate(&chimeraData.markerGenerationPerPilotProfiling);
                            #endif
                            MarkersGeneration(&chimeraData, FAST_CHANNEL, prn,pilotSymbolsToGenerate);
                            #ifdef CHIMERA_PROFILING
                            StopProfilingUpdate(&chimeraData.markerGenerationPerPilotProfiling,pilotSymbolsToGenerate);
                            #endif
                            printf("TOI = %d, GPS PRN %d: %.1f seconds of Fast Marker generated\n",toi,prn,pilotSymbolsToGenerate*0.01);

                        }
                        else
                        {
                            //SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"GPS PRN %d: Digital Signature NOT verified",prn);
                            printf("TOI = %d, GPS PRN %d: Digital Signature NOT verified\n",toi,prn);
                        }
                        byteIdxOut[prn-1]=0;
                        bitIdxOut[prn-1]=0;
                        pageIndex[prn-1]=0;
                        ArrayClean((char *)chimeraEpoch[prn-1],NUMBER_OF_BYTES_IN_A_MESSAGE*NUMBER_OF_MESSAGE_IN_A_EPOCH);
                        
                    }
            
                }
            }
		}
	}


	// deinit
    DeinitChimeraData(&chimeraData);
	for(prn=0;prn<32;prn++)
    {
		free(message[prn]);
        free(chimeraEpoch[prn]);
    }
	fclose(msg);
	free(ngeneXmmsBackup);

	/**********************************************************************/
    
	return 0;


}

