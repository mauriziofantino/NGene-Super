/**
 * @file osnmaManagement.c
 *
 * @author Mario Nicola
 *
 * @date 16/08/2014
 *
 * @version 1.0.0
 *
 * @brief Some basic functions for the OSNMA management.
 */

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "osnmaManagement.h"
#include "ngeneConstants.h"
#include "timeConstants.h"
#include "navigationMessageParser.h"
#include "osnmaUtilities.h"
#include "mackList.h"
#include "dsmData.h"
#include "navigationMessageParser.h"
#include "arrays.h"
#include "ioFunctions.h"
#include "ncursesDataStructure.h"
#include "ngeneConfiguration.h"
#include "closeManager.h"
#include "macLookupTable.h"
#include "channelManager.h"
#include "timeConversions.h"
#include "osnmaValidations.h"


/**/
static TTeslaKeyVerificationThreadFunctionParametersStruct teslaKeyVerificationThFuncParam; /**< Parameters structure of the threadTESLAKeyVerification*/
static pthread_t threadTESLAKeyVerification; /**< Thread handling the accomplishment of the TESLA key verification */
extern int threadTESLAKeyVerificationStopIsRunning;
/**/



void InitOsnmaParameters(TNGeneConfiguration *cfg,TOsnmaParameters *osnma)
{
	char cfgFilename[MAX_ROW_LENGTH];
	char tag[MAX_TAG_LEN];
	char string[MAX_ROW_LENGTH];
	char osnmaFolder[MAX_ROW_LENGTH];
	int idx;
	FILE *mtRootFile;
	unsigned int byte;
	//**
	char *fileExt=NULL;
	size_t sizeMtRoot=0;
	int lineCount=0;

	osnma->parametersValid = FALSE_FLAG;
	/* All variables initialize to not-valid values */
	osnma->chainId = -1;
	osnma->pkId = -1;
	osnma->dsmKrootNb = -1;
	osnma->nmack = -1;
	osnma->hashFunction = HF_RSVD;
	osnma->macFunction = MF_3_RSVD;
	osnma->keySize = -1;
	osnma->keySizeBytes=-1;
	osnma->macSize = -1;
	osnma->macLutIndex = -1;
	osnma->mackOffset = 0;
	osnma->macSizeBytes = -1;
	osnma->mackBlockDuration = -1;
	osnma->macPerMackBlock = -1;
	osnma->dsmId = -1;
	osnma->dsmPkrNb = -1;
	osnma->dsmPkrMessageId = -1;
	osnma->dsmPkrNpkt = NPKT_RSVD;
	osnma->dsmPkrNpkId = -1;

	/**/
	osnma->MSize = -1;
	osnma->DSSizeBytes=-1;
    osnma->DSSizeBits=-1;
	osnma->M_DSSizeBytes = -1;
    osnma->M_DSSizeBits = -1;
	osnma->LBits = -1;
	osnma->LBytes = -1;

	/* Get parameters from the OSNMA configuration file  */
	/* prepare the file name */
	cfgFilename[0] = '\0';
	strcat(cfgFilename, cfg->configurationPath);
	strcat(cfgFilename, "OSNMA_parameters.txt");
	/* Parse parameters */
	strcpy(tag,"MACK Block Offset");
	if(GetVal(cfgFilename,tag,(void *)&(osnma->mackBlockOffset),F_INTEGER,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	if((osnma->mackBlockOffset>1) || (osnma->mackBlockOffset<0))
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> has invalid value",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}	
	/**/
	strcpy(tag, "max NS");
	if(GetVal(cfgFilename,tag,(void *)&(osnma->maxNs),F_INTEGER,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	/**/
	strcpy(tag, "Minimum MAC length");
	if(GetVal(cfgFilename,tag,(void *)&(osnma->minimumMacSize),F_INTEGER,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	osnma->requiredMacAccumulations = 0;
	/**/
	strcpy(tag, "OSNMA timeout");
	if(GetVal(cfgFilename,tag,(void *)&(osnma->osnmaTimeout),F_INTEGER,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_WARNING,"Cannot find <%s> in <%s>: OSNMA timeout disabled",tag,cfgFilename);
		osnma->osnmaTimeout = OSNMA_TIMEOUT_DISABLED;
	}
	if(osnma->osnmaTimeout<=OSNMA_TIMEOUT_MINIMUM)
	{
		osnma->osnmaTimeout = OSNMA_TIMEOUT_DISABLED;
	}
	/**/
	strcpy(tag, "OSNMA data folder");
	if(GetVal(cfgFilename,tag,(void *)osnmaFolder,F_STRING,1) == FAILURE_RETURN)
	{
		strcpy(osnmaFolder,cfg->otherDataPath);
		SwrxLog(NGENE_LOG_SEVERITY_WARNING,"Cannot find OSNMA parameters' folder. Use default files in %s",osnmaFolder);
	}
	else
	{
		// check that the folder name terminates with '/'
		idx = strlen(osnmaFolder);
		if(osnmaFolder[idx-1]!='/')
		{
			osnmaFolder[idx] = '/';
			osnmaFolder[idx+1] = '\0'; // terminate the string
		}
		SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"OSNMA parameters from the folder %s",osnmaFolder);
	}
	/**/
	strcpy(tag, "Public key filename");
	if(GetVal(cfgFilename,tag,(void *)string,F_STRING,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	osnma->publicKeyFilename[0] = '\0';
	strcat(osnma->publicKeyFilename, osnmaFolder);
	strcat(osnma->publicKeyFilename, string);
	if(CheckFileExistence(osnma->publicKeyFilename)==FALSE_FLAG)
	{
		SwrxLog(NGENE_LOG_SEVERITY_WARNING,"OSNMA: specified Public Key file does not exist. Use the default one in %s",cfg->otherDataPath);
		osnma->publicKeyFilename[0] = '\0';
		strcat(osnma->publicKeyFilename, cfg->otherDataPath);
		strcat(osnma->publicKeyFilename, string);
	}
	/**/
	strcpy(tag, "Merkle Tree Root filename");
	if(GetVal(cfgFilename,tag,(void *)string,F_STRING,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	osnma->mtRootFilename[0] = '\0';
	strcat(osnma->mtRootFilename, osnmaFolder);
	strcat(osnma->mtRootFilename, string);
	if(CheckFileExistence(osnma->mtRootFilename)==FALSE_FLAG)
	{
		SwrxLog(NGENE_LOG_SEVERITY_WARNING,"OSNMA: specified Merkle Tree Root file does not exist. Use the default one in %s",cfg->otherDataPath);
		osnma->mtRootFilename[0] = '\0';
		strcat(osnma->mtRootFilename, cfg->otherDataPath);
		strcat(osnma->mtRootFilename, string);
	}
	// read the Root of the Merkle tree
	mtRootFile = fopen(osnma->mtRootFilename,"r");
	if(mtRootFile!=NULL)
	{
		/*Check the extension*/
		fileExt = strrchr(osnma->mtRootFilename, '.');
		if (!fileExt)
		{
		    /* no extension */
			SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The Merkle Tree file has no extension");
			CloseNGene(cfg,EXIT_FAILURE);
		}
		if(strcmp((fileExt + 1), "txt")==0 || strcmp((fileExt + 1), "TXT")==0)
		{
			for(idx=0;idx<(MERKLE_TREE_NODE_SIZE/BITS_PER_BYTE);idx++)
			{
				// read a byte at each iteration
				if(fscanf(mtRootFile,"%2x",&byte)==1)
					osnma->mtRoot[idx] = (unsigned char)byte;
				else
					osnma->mtRoot[idx] = 0x00;
			}
		}
		else if (strcmp((fileExt + 1), "xml")==0 || strcmp((fileExt + 1), "XML")==0)
		{
			  lineCount=GetLineCountXml(mtRootFile);
			  //*********************
			  fclose(mtRootFile);
			  mtRootFile = fopen(osnma->mtRootFilename,"r");
			  //**********************
			  if(lineCount>1)
			  {
				  strcpy(tag, TAG_MT_NODE);
				  if(GetValXml(mtRootFile, tag, (void *)&(osnma->mtRoot), &sizeMtRoot) == FAILURE_RETURN)
				  {
						SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",osnma->mtRootFilename,tag);
						CloseNGene(cfg,EXIT_FAILURE);
				  }
			  }
			  else
			  {
				  strcpy(tag, MT_ROOT_NODE_START);
				  if(GetValXml(mtRootFile, tag, (void *)&(osnma->mtRoot), &sizeMtRoot) == FAILURE_RETURN)
				  {
						SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",osnma->mtRootFilename,tag);
						CloseNGene(cfg,EXIT_FAILURE);
				  }
			  }
			  if(sizeMtRoot!=SHA256_DIGEST_LENGTH)
			  {
					SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",osnma->mtRootFilename,tag);
					CloseNGene(cfg,EXIT_FAILURE);
			  }
		}
		else
		{
		    /* no extension */
			SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The Merkle Tree file has no valid extension, please use only .txt or .xml format files");
			CloseNGene(cfg,EXIT_FAILURE);
		}
		fclose(mtRootFile);
	}
	/**/
	strcpy(tag, "MAC Lookup Table filename");
	if(GetVal(cfgFilename,tag,(void *)string,F_STRING,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	osnma->macLutFilename[0] = '\0';
	strcat(osnma->macLutFilename, osnmaFolder);
	strcat(osnma->macLutFilename, string);
	if(CheckFileExistence(osnma->macLutFilename)==FALSE_FLAG)
	{
		SwrxLog(NGENE_LOG_SEVERITY_WARNING,"OSNMA: specified MAC LUT file does not exist. Use the default one in %s",cfg->otherDataPath);
		osnma->macLutFilename[0] = '\0';
		strcat(osnma->macLutFilename, cfg->otherDataPath);
		strcat(osnma->macLutFilename, string);
	}
	InitMacLut(osnma);
	/**/
	strcpy(tag, "Keys Index Table filename");
	if(GetVal(cfgFilename,tag,(void *)string,F_STRING,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	osnma->keyIndexTableFilename[0] = '\0';
	strcat(osnma->keyIndexTableFilename, osnmaFolder);
	strcat(osnma->keyIndexTableFilename, string);
	if(CheckFileExistence(osnma->keyIndexTableFilename)==FALSE_FLAG)
	{
		SwrxLog(NGENE_LOG_SEVERITY_WARNING,"OSNMA: specified TESLA Keys Index Table file does not exist. Use the default one in %s",cfg->otherDataPath);
		osnma->keyIndexTableFilename[0] = '\0';
		strcat(osnma->keyIndexTableFilename, cfg->otherDataPath);
		strcat(osnma->keyIndexTableFilename, string);
	}
	/**/
	strcpy(tag, "curr NS");
	if(GetVal(osnma->keyIndexTableFilename,tag,(void *)&(osnma->ns),F_INTEGER,1) == FAILURE_RETURN)
	{
		/*  if "curr NS" is not  found, try the old flag */
		strcpy(tag, "NS");
		if(GetVal(osnma->keyIndexTableFilename,tag,(void *)&(osnma->ns),F_INTEGER,1) == FAILURE_RETURN)
		{
			SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",osnma->keyIndexTableFilename,tag);
			CloseNGene(cfg,EXIT_FAILURE);
		}
	}
	/**/
	osnma->keyIndexTable = (int *)malloc(osnma->maxNs*sizeof(int));
	strcpy(tag, "Key indexes");
	if(GetVal(osnma->keyIndexTableFilename,tag,(void *)(osnma->keyIndexTable),F_INTEGER,osnma->maxNs) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",osnma->keyIndexTableFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	for(idx=0;idx<osnma->maxNs;idx++)
	{
		if((osnma->keyIndexTable[idx]<1) || (osnma->keyIndexTable[idx]>osnma->ns))
		{
			SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: Table of key indexes not valid",osnma->keyIndexTableFilename);
			CloseNGene(cfg,EXIT_FAILURE);
		}
	}
}


void DeinitOsnmaParameters(TOsnmaParameters *osnma)
{
	free(osnma->keyIndexTable);
	DeinitMacLut(osnma);
}


void GetOsnmaParameters(TOsnmaParameters *osnma,unsigned char *chDsm,int overwrite,FILE *filePtr)
{
	int dsmid;
	int bid;

	if((osnma->parametersValid == TRUE_FLAG) && (overwrite==FALSE_FLAG))
	{
		return;
	}
	dsmid = (chDsm[1]>>4) & 0x0f;
	bid = chDsm[1] & 0x0f;
	if((GetDsmTypeFromDsmId(dsmid)!=DSM_KROOT) || (bid!=0))
	{
		return;
	}
	osnma->parametersValid = TRUE_FLAG;
	/* save NMA Header */
	osnma->nmaHeader = chDsm[0];
	/* get parameters */
	osnma->dsmKrootNb = GetNumberOfBlocks(ExtractField(chDsm,2,2,4,0));
	osnma->pkId = ExtractField(chDsm,2,2,0,4);
	osnma->chainId = ExtractField(chDsm,3,3,2,0);
	osnma->nmack =  ExtractField(chDsm,3,3,4,6);
	osnma->hashFunction = ExtractField(chDsm,3,3,6,4);
	GetHashFunctionForTeslaKeyVerification(osnma);
	osnma->macFunction = ExtractField(chDsm,3,3,0,2);
	osnma->keySize = GetTeslaChainKeySize(ExtractField(chDsm,4,4,4,0));
	osnma->keySizeBytes=osnma->keySize/8;
	osnma->macSize = GetMacSize(ExtractField(chDsm,4,4,0,4));
	osnma->macLutIndex = chDsm[5];
	osnma->mackOffset = ExtractField(chDsm,6,6,4,6);
	osnma->krootWn = ExtractField(chDsm,6,7,4,8);
	osnma->krootTow = ((int)chDsm[8])*SECONDS_PER_HOUR;
	memcpy (osnma->alpha, &chDsm[9], 6);
	osnma->dsmId = dsmid;
	/* complete the initialization */
	osnma->macSizeBytes = (int)ceil((double)osnma->macSize/(double)BITS_PER_BYTE);
	if(osnma->nmack!=0)
	{
		osnma->mackBlockSize = MACK_PER_SUBFRAME_SIZE_BITS/osnma->nmack;
		osnma->mackBlockSizeBytes = osnma->mackBlockSize/BITS_PER_BYTE;
		osnma->mackBlockDuration = GAL_SECONDS_PER_SUBFRAME/osnma->nmack;
		osnma->macPerMackBlock = (int)floor((double)((MACK_PER_SUBFRAME_SIZE_BITS/osnma->nmack)-osnma->keySize)/(double)(osnma->macSize+16));
	}
	else
	{
		/* nmack==0 should be reserved value... */
		osnma->mackBlockSize = MACK_PER_SUBFRAME_SIZE_BITS;
		osnma->mackBlockSizeBytes = osnma->mackBlockSize/BITS_PER_BYTE;
		osnma->mackBlockDuration = GAL_SECONDS_PER_SUBFRAME;
		osnma->macPerMackBlock = (int)floor((double)(MACK_PER_SUBFRAME_SIZE_BITS-osnma->keySize)/(double)(osnma->macSize+16)); /* See section 4 of ICD v1.1 */
	}
	LoadMacSequence(osnma);
	osnma->requiredMacAccumulations = (int)ceil((double)osnma->minimumMacSize/(double)osnma->macSize);

	/* get a feedback about parameters */
	if(overwrite==TRUE_FLAG)
	{
		SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"OSNMA parameters found!!!");
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"    NB            = %d",osnma->dsmKrootNb);
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"    PKID          = %d",osnma->pkId);
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"    Chain ID      = %d",osnma->chainId);
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"    NMACK         = %d",osnma->nmack);
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"    Hash function = %d",osnma->hashFunction);
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"    MAC function  = %d",osnma->macFunction);
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"    Key size      = %d",osnma->keySize);
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"    MAC size      = %d",osnma->macSize);
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"    MAC LUT index = %d",osnma->macLutIndex);
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"    MACK offset   = %d",osnma->mackOffset);
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"    KROOT WN      = %d",osnma->krootWn);
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"    KROOT TOW     = %d",osnma->krootTow);
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"    alpha         = %02x%02x %02x%02x %02x%02x",osnma->alpha[0],osnma->alpha[1],osnma->alpha[2],osnma->alpha[3],osnma->alpha[4],osnma->alpha[5]);
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"OSNMA other parameters");
		//#ifndef OSNMA_EXTRA_KEY_DELAY
		if(osnma->mackBlockOffset==0)
		{
			SwrxLog(NGENE_LOG_SEVERITY_INFO,"    NS            = %d\n",osnma->ns);
		}
		else
		{
			SwrxLog(NGENE_LOG_SEVERITY_INFO,"    NS            = %d",osnma->ns);
			SwrxLog(NGENE_LOG_SEVERITY_INFO,"    MACK block offset");
		}
		SwrxLog(NGENE_LOG_SEVERITY_INFO,"    DSM ID        = %d\n",osnma->dsmId);
	}
	
	/* perform a validity check of the parsed parameters */
	if((osnma->nmaHeader==0x00) || 
		(osnma->nmack==0) ||
		(osnma->hashFunction==HF_RSVD) ||
		(osnma->macFunction>=MF_2_RSVD) ||
		(osnma->keySize<=0) ||  //  invalid value set by GetTeslaChainKeySize: -1
		(osnma->macSize<=0) || // invalid value set by GetMacSize: -1
		(osnma->mackOffset>=2))
		{
			/* if the parameters are not valid, report and exit */
			SwrxLog(NGENE_LOG_SEVERITY_WARNING,"Discarded invalid OSNMA parameters\n");
			osnma->parametersValid = FALSE_FLAG;
			return;
		}
	
	/* the parameters are good, continue */
	if((overwrite==TRUE_FLAG) && (filePtr!=NULL))
	{
		/* on text file */
		fprintf(filePtr,"%d\n",osnma->dsmKrootNb);
		fprintf(filePtr,"%d\n",osnma->pkId);
		fprintf(filePtr,"%d\n",osnma->chainId);
		fprintf(filePtr,"%d\n",osnma->nmack);
		fprintf(filePtr,"%d\n",osnma->hashFunction);
		fprintf(filePtr,"%d\n",osnma->macFunction);
		fprintf(filePtr,"%d\n",osnma->keySize);
		fprintf(filePtr,"%d\n",osnma->macSize);
		fprintf(filePtr,"%d\n",osnma->macLutIndex);
		fprintf(filePtr,"%d\n",osnma->mackOffset);
		fprintf(filePtr,"%d\n",osnma->krootWn);
		fprintf(filePtr,"%d\n",osnma->krootTow);
		fprintf(filePtr,"%02x%02x%02x%02x%02x%02x\n",osnma->alpha[0],osnma->alpha[1],osnma->alpha[2],osnma->alpha[3],osnma->alpha[4],osnma->alpha[5]);
		fprintf(filePtr,"%d\n",osnma->ns);
		fprintf(filePtr,"%d\n",osnma->mackBlockOffset);
		fflush(filePtr);
	}
	/**/
	osnma->MSize=13+osnma->keySizeBytes;
    osnma->M_DSSizeBits=osnma->MSize*8+osnma->DSSizeBits;
    osnma->M_DSSizeBytes=ceil((double)osnma->M_DSSizeBits/8);
	osnma->LBits=(104*osnma->dsmKrootNb)-osnma->M_DSSizeBits;
	osnma->LBytes=ceil((double)osnma->LBits/8);
}


void CopyOsnmaParameters(TOsnmaParameters *src,TOsnmaParameters *dst,int overwriteKRootTime)
{
	dst->parametersValid = TRUE_FLAG;
	dst->nmaHeader = src->nmaHeader;
	dst->dsmKrootNb = src->dsmKrootNb;
	dst->pkId = src->pkId;
	dst->chainId = src->chainId;
	dst->nmack =  src->nmack;
	dst->hashFunction = src->hashFunction;
	GetHashFunctionForTeslaKeyVerification(dst);
	dst->macFunction = src->macFunction;
	dst->keySize = src->keySize ;
	dst->keySizeBytes=dst->keySize/8;
	dst->macSize = src->macSize;
	dst->macLutIndex = src->macLutIndex;
	dst->mackOffset = src->mackOffset;
	if(overwriteKRootTime==TRUE_FLAG)
	{
		dst->krootWn = src->krootWn;
		dst->krootTow = src->krootTow;
	}
	memcpy (dst->alpha, src->alpha, 6);
	dst->dsmId = src->dsmId;
	/* complete the initialization */
	dst->macSizeBytes = src->macSizeBytes ;
	dst->mackBlockSize = src->mackBlockSize;
	dst->mackBlockSizeBytes = src->mackBlockSizeBytes;
	dst->mackBlockDuration = src->mackBlockDuration;
	dst->macPerMackBlock = src->macPerMackBlock;
	LoadMacSequence(dst);
	dst->minimumMacSize = src->minimumMacSize;
	dst->requiredMacAccumulations = src->requiredMacAccumulations;
	dst->MSize = src->MSize;
    // these parameters could be wrong if the size of PK/DS is changed!
	dst->M_DSSizeBits = src->M_DSSizeBits;
    dst->M_DSSizeBytes = src->M_DSSizeBytes;
	dst->LBits = src->LBits;
	dst->LBytes = src->LBytes;
}


void InitOsnmaData(TNGeneConfiguration *cfg,TOsnmaData *osnma)
{
	FILE *PKRFile;
	//size_t pubKeyCompressedSize;
	int index, indexRow, indexColumn;
	//unsigned hex;
	//int nid;
	int DSMaxSize;
	//unsigned char pubKeyCompressed[MAX_COMPRESSED_PUBLICKEY_SIZE];
	//**
	int NMA_Status_Header_SizeBits=2;/*Size of NMA Status (bits)*/
	unsigned int P4_max;/*Maximum number of Padding LBits (0 to 7 bits)*/
	int maxNavDataSizeBits=MAX_NAVDATA_BITS_FOR_MAC_VERIFICATION; /*Maximum number of Data to be authenticated (bits)*/
	int GST_SFSize=4; /*Size of GST_SF (bytes)*/
	char filename[MAX_ROW_LENGTH];

	/**/
	char * fileExt;
	char tag[MAX_TAG_LEN];
	unsigned char pkrPublicKey[MAX_ROW_LENGTH]; /*Raw binary compressed ECDSA Public Key*/
	/*Type of the compressed ECDSA Public Key*/
	int pkrNpktType;
	TNewPublicKeyType pkrNpkt=ECDSA_P224;
	size_t pkrNpkLen=0;
	/**/

	osnma->osnmaTransmitted = FALSE_FLAG;
	osnma->revocationStarted = FALSE_FLAG;
	if(cfg->receiverClockEmulation==TRUE_FLAG)
	{
		FromPcTimeToGalileoTime(NULL,FALSE_FLAG,cfg->navMsgConfiguration.defaultLeapSeconds,&(osnma->receiverClockWn),&(osnma->receiverClockTow));
		osnma->receiverClockError = cfg->receiverClockError;
	}
	else
	{
		osnma->receiverClockWn = -1;
		osnma->receiverClockTow = -1.0;
		osnma->receiverClockError = 0;
	}
	InitOsnmaParameters(cfg,&(osnma->osnmaPars));
	InitOsnmaParameters(cfg,&(osnma->osnmaParsNew));
	osnma->mackList = NULL; /* empty */
	InitDsmData(&(osnma->dsmData),cfg->numberOfChannels);
	osnma->navdataBitsForMacLen = -1; /* no valid data in navdataBitsForMac array */
	if(cfg->logConfiguration.enableOsnmaLog==TRUE_FLAG)
	{
		filename[0] = '\0';
		strcat(filename, cfg->dumpPathCurrent);
		strcat(filename, "/osnmaParametersLog.txt");
		osnma->parsLogFile = fopen(filename,"w");
		//
		filename[0] = '\0';
		strcat(filename, cfg->dumpPathCurrent);
		strcat(filename, "/osnmaMacLog.txt");
		osnma->macLogFile = fopen(filename,"w");
		//
		filename[0] = '\0';
		strcat(filename, cfg->dumpPathCurrent);
		strcat(filename, "/osnmaMacseqLog.txt");
		osnma->macseqLogFile = fopen(filename,"w");
		//
		filename[0] = '\0';
		strcat(filename, cfg->dumpPathCurrent);
		strcat(filename, "/osnmaTeslaChainRaw.txt");
		osnma->teslaChainFile = fopen(filename,"w");
		//
		filename[0] = '\0';
		strcat(filename, cfg->dumpPathCurrent);
		strcat(filename, "/osnmaDsmLog.txt");
		osnma->dsmLogFile = fopen(filename,"w");
		//
		filename[0] = '\0';
		strcat(filename, cfg->dumpPathCurrent);
		strcat(filename, "/osnmaStatusNGene.txt");
		osnma->osnmaStatusLogFile = fopen(filename,"w");
		//
		filename[0] = '\0';
		strcat(filename, cfg->dumpPathCurrent);
		strcat(filename, "/osnmaNavDataInMacAuth.txt");
		osnma->navdataLogFile = fopen(filename,"w");
		//
		filename[0] = '\0';
		strcat(filename, cfg->dumpPathCurrent);
		strcat(filename, "/osnmaValidations.txt");
		osnma->validationLogFile = fopen(filename,"w");
		//
		filename[0] = '\0';
		strcat(filename, cfg->dumpPathCurrent);
		strcat(filename, "/osnmaAuthentications.txt");
		osnma->authLogFile = fopen(filename,"w");
	}
	else
	{
		osnma->parsLogFile = NULL;
		osnma->macLogFile = NULL;
		osnma->macseqLogFile = NULL;
		osnma->teslaChainFile = NULL;
		osnma->dsmLogFile = NULL;
		osnma->osnmaStatusLogFile = NULL;
		osnma->navdataLogFile = NULL;
		osnma->validationLogFile = NULL;
		osnma->authLogFile = NULL;
	}
	if(cfg->logConfiguration.enableOsnmaSimpleLog==TRUE_FLAG)
	{
		filename[0] = '\0';
		strcat(filename, cfg->dumpPathCurrent);
		strcat(filename, "/osnmaLog.txt");
		osnma->simpleLogFile = fopen(filename,"w");
		//
		filename[0] = '\0';
		strcat(filename, cfg->dumpPathCurrent);
		strcat(filename, "/osnmaLog.bin");
		osnma->simpleLogBinFile = fopen(filename,"wb");
	}
	else
	{
		osnma->simpleLogFile = NULL;
		osnma->simpleLogBinFile = NULL;
	}
	osnma->rxOsnmaStatusLastPrint = RX_OSNMA_NOT_PRESENT;
	osnma->lastSimpleLogSaving = INVALID_VALUE;
	#ifdef OSNMA_PROFILING
	InitProfilingSupport(&(osnma->dsmKrootProfiling),"DSM KROOT Verification");
	InitProfilingSupport(&(osnma->P1Verification),"P1 field in DSM KROOT Verification");
	InitProfilingSupport(&(osnma->merkleTreeVerification),"Public Key Verification through Merkle Tree");
	InitProfilingSupport(&(osnma->P2Verification),"P2 field in DSM PKR Verification");
	InitProfilingSupport(&(osnma->pubPEMToEVPKey),"Uploading of the public key from .pem file");
	InitProfilingSupport(&(osnma->teslaProfiling),"TESLA Key Verification");
	InitProfilingSupport(&(osnma->oneStepOfTeslaChainProfiling),"One Step of TESLA Chain Verification (single key)");
	InitProfilingSupport(&(osnma->allMacsProfiling),"All MACs Verification");
	InitProfilingSupport(&(osnma->macProfiling),"MAC Verification (single MAC)");
	InitProfilingSupport(&(osnma->macSeqProfiling),"MACSEQ Verification (single MACSEQ)");
	InitProfilingSupport(&(osnma->saveMackInMackList),"Save Mack In Mack List function");
	InitProfilingSupport(&(osnma->saveMackInMackListExcludingTESLA),"Save Mack In Mack List function excluding the TESLA Chain Verification");
	InitProfilingSupport(&(osnma->saveNavdataInMackList),"Save Navigation Data In Mack List function");
	InitProfilingSupport(&(osnma->saveDsmBlock),"Save DSM Block function");
	InitProfilingSupport(&(osnma->extractAndCheckBitsForMACVerification),"Extract and Check Bits for MAC Verification function");
	InitProfilingSupport(&(osnma->findKeyForMACVerification),"Find Key for MAC Verification");
	InitProfilingSupport(&(osnma->mainLoop),"Main loop excluding the TESLA Key Verification");
	#endif

	/*READ THE PUBLIC KEY FROM FILE*/
	osnma->evp_key=NULL;
	PKRFile=fopen(osnma->osnmaPars.publicKeyFilename,"r");
	if(PKRFile!=NULL)
	{
		/*Check the extension*/
		fileExt = strrchr(osnma->osnmaPars.publicKeyFilename, '.');
		if (!fileExt)
		{
		    /* no extension */
			SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The Public Key file has no extension");
			CloseNGene(cfg,EXIT_FAILURE);
		}
		if(strcmp((fileExt + 1), "pem")==0 || strcmp((fileExt + 1), "PEM")==0)
		{
			#ifdef OSNMA_PROFILING
			StartProfilingUpdate(&(osnma->pubPEMToEVPKey));
			#endif
			osnma->evp_key=PublicKey_PEMToEVPKEY(PKRFile);
			#ifdef OSNMA_PROFILING
			StopProfilingUpdate(&(osnma->pubPEMToEVPKey),1);
			#endif
			osnma->evp_keyPkid = INVALID_VALUE; // not available from the standard PEM format
		}
		else if (strcmp((fileExt + 1), "xml")==0 || strcmp((fileExt + 1), "XML")==0)
		{
			  strcpy(tag, TAG_LENGTH);
			  if(GetValXml(PKRFile, tag, (void *)&(pkrNpktType), NULL) == FAILURE_RETURN)
			  {
					SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",osnma->osnmaPars.publicKeyFilename,tag);
					CloseNGene(cfg,EXIT_FAILURE);
			  }
			  //**
				switch(pkrNpktType)
				{
				case ECDSA_COMPRESSED_KEY_LEN_P224:
					pkrNpkt=ECDSA_P224;
					break;
				case ECDSA_COMPRESSED_KEY_LEN_P256:
					pkrNpkt=ECDSA_P256;
					break;
				case ECDSA_COMPRESSED_KEY_LEN_P384:
					pkrNpkt=ECDSA_P384;
					break;
				case ECDSA_COMPRESSED_KEY_LEN_P521:
					pkrNpkt=ECDSA_P521;
					break;
				default:
					pkrNpkt=ECDSA_P224;
					break;
				}
			  //**
				//*********************
				//fclose(PKRFile);
				//PKRFile=fopen(osnma->osnmaPars.publicKeyFilename,"r");
				rewind(PKRFile);
				//**********************
			  strcpy(tag, TAG_EXADECIMAL_STRING_START);
			  if(GetValXml(PKRFile, tag, (void *)&(pkrPublicKey), &pkrNpkLen) == FAILURE_RETURN)
			  {
					SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",osnma->osnmaPars.publicKeyFilename,tag);
					CloseNGene(cfg,EXIT_FAILURE);
			  }
			  if(osnma->evp_key==NULL)
			  {
				  osnma->evp_key= EVP_PKEY_new();
			  }
			  PublicKey_PkrToEVPKEY(pkrNpkt, pkrPublicKey,pkrNpkLen,osnma->evp_key);
			  // try to get the PKID
			  rewind(PKRFile);
			  strcpy(tag, TAG_PKID);
			  if(GetValXml(PKRFile, tag, (void *)&(osnma->evp_keyPkid), NULL) == FAILURE_RETURN)
			  {
				  // not found, set to INVALID_VALUE
				  osnma->evp_keyPkid = INVALID_VALUE;
			  }
		}
		else
		{
		    /* no extension */
			SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The Public Key file has no valid extension, please use only .pem or .xml format files");
			CloseNGene(cfg,EXIT_FAILURE);
		}
		osnma->evp_keyValid = TRUE_FLAG;
		DSMaxSize=EVP_PKEY_size(osnma->evp_key);
		switch(DSMaxSize)
		{
		case 64:
			//pubKeyUncompressedSize=57;
			//pubKeyCompressedSize=29;
			osnma->function=EVP_sha224();
			//				hashSize=SHA224_DIGEST_LENGTH;
			//nid=NID_secp224r1;
            osnma->osnmaPars.DSSizeBits=448;
			break;
		case 72:
			//pubKeyUncompressedSize=65;
			//pubKeyCompressedSize=33;
			osnma->function=EVP_sha256();
			//hashSize=SHA256_DIGEST_LENGTH;
			//nid=NID_X9_62_prime256v1;
			osnma->osnmaPars.DSSizeBits=512;
			break;
		case 104:
			//pubKeyUncompressedSize=97;
			//pubKeyCompressedSize=49;
			osnma->function=EVP_sha384();
			//hashSize=SHA384_DIGEST_LENGTH;
			//nid=NID_secp384r1;
			osnma->osnmaPars.DSSizeBits=768;
			break;
		case 141:
			//pubKeyUncompressedSize=133;
			//pubKeyCompressedSize=67;
			osnma->function=EVP_sha512();
			//hashSize=SHA512_DIGEST_LENGTH;
			//nid=NID_secp521r1;
            #ifdef PATCH_JRC_GENERATOR_DSM
            osnma->osnmaPars.DSSizeBits=1042;
            #else
            osnma->osnmaPars.DSSizeBits=1056;
            #endif
			break;
		default:
			//pubKeyUncompressedSize=57;
			//pubKeyCompressedSize=29;
			osnma->function=EVP_sha224();
			//hashSize=SHA224_DIGEST_LENGTH;
			//nid=NID_secp224r1;
			osnma->osnmaPars.DSSizeBits=448;
			break;
		}
        osnma->osnmaPars.DSSizeBytes=osnma->osnmaPars.DSSizeBits/8;
        #ifdef PATCH_JRC_GENERATOR_DSM
        if(DSMaxSize==141)
        {
            osnma->osnmaPars.DSSizeBytes= 132;
        }
        #endif
        // init also parsNew
        osnma->osnmaParsNew.DSSizeBits = osnma->osnmaPars.DSSizeBits;
        osnma->osnmaParsNew.DSSizeBytes = osnma->osnmaPars.DSSizeBytes;
        fclose(PKRFile);
	}
	else
	{
		/* WARNING: seemingly, PEM_read_PUBKEY called by PublicKey_PEMToEVPKEY allocates the memory for the public key
		 *          so it is not necessary to explicitly allocate memory for the public key with EVP_PKEY_new
		 *          when the PEN file is available and PublicKey_PEMToEVPKEY is called.
		 *          In this case, EVP_PKEY_new is used, the allocated memory is flagged as a leakage at the end of execution
		 *          (the pointer is overwritten by PublicKey_PEMToEVPKEY) */

		osnma->evp_keyPkid = INVALID_VALUE;
		osnma->evp_keyValid = FALSE_FLAG;
		/* Allocates an empty EVP_PKEY structure which is used by OpenSSL to store public key
		 * only when the PEM file is not available */
		if ((osnma->evp_key= EVP_PKEY_new()) == NULL)
		{
			//return 1;
		}
		/* cannot find public key from file, create a default one */
		SwrxLog(NGENE_LOG_SEVERITY_WARNING,"Cannot read the public key from the specified file (%s). A default one is loaded...",osnma->osnmaPars.publicKeyFilename);
		//pubKeyCompressedSize = 29;
		//ArrayClean((char *)pubKeyCompressed,MAX_COMPRESSED_PUBLICKEY_SIZE);
		osnma->function=EVP_sha224();
		//nid=NID_secp224r1;
		osnma->osnmaPars.DSSizeBytes=56;
	}



//	PKFile=fopen(osnma->osnmaPars.publicKeyFilename,"r");
//	if(PKFile!=NULL)
//	{
//		pubKeyCompressedSize=0;
//		index=0;
//		while(1==fscanf(PKFile, "%2x", &hex))
//		{
//			pubKeyCompressed[index] = (unsigned char)hex;
//			index++;
//		}
//		pubKeyCompressedSize=index;
//		switch(pubKeyCompressedSize)
//		{
//		case 29:
//			//pubKeyUncompressedSize=57;
//			//pubKeyCompressedSize=29;
//			osnma->function=EVP_sha224();
//			//				hashSize=SHA224_DIGEST_LENGTH;
//			nid=NID_secp224r1;
//			osnma->osnmaPars.DSSizeBytes=56;
//			break;
//		case 33:
//			//pubKeyUncompressedSize=65;
//			//pubKeyCompressedSize=33;
//			osnma->function=EVP_sha256();
//			//hashSize=SHA256_DIGEST_LENGTH;
//			nid=NID_X9_62_prime256v1;
//			osnma->osnmaPars.DSSizeBytes=64;
//			break;
//		case 49:
//			//pubKeyUncompressedSize=97;
//			//pubKeyCompressedSize=49;
//			osnma->function=EVP_sha384();
//			//hashSize=SHA384_DIGEST_LENGTH;
//			nid=NID_secp384r1;
//			osnma->osnmaPars.DSSizeBytes=96;
//			break;
//		case 67:
//			//pubKeyUncompressedSize=133;
//			//pubKeyCompressedSize=67;
//			osnma->function=EVP_sha512();
//			//hashSize=SHA512_DIGEST_LENGTH;
//			nid=NID_secp521r1;
//			osnma->osnmaPars.DSSizeBytes=132;
//			break;
//		default:
//			//pubKeyUncompressedSize=57;
//			//pubKeyCompressedSize=29;
//			osnma->function=EVP_sha224();
//			//hashSize=SHA224_DIGEST_LENGTH;
//			nid=NID_secp224r1;
//			osnma->osnmaPars.DSSizeBytes=56;
//			break;
//		}
//		fclose(PKFile);
//	}
//	else
//	{
//		/* cannot find public key from file, create a default one */
//		SwrxLog(NGENE_LOG_SEVERITY_WARNING,"Cannot read the public key from the specified file (%s). A default one is loaded...",osnma->osnmaPars.publicKeyFilename);
//		pubKeyCompressedSize = 29;
//		ArrayClean((char *)pubKeyCompressed,MAX_COMPRESSED_PUBLICKEY_SIZE);
//		osnma->function=EVP_sha224();
//		nid=NID_secp224r1;
//		osnma->osnmaPars.DSSizeBytes=56;
//	}

	size_t M_DSSizeMax,LBitsMax,LBytesMax,M_DSSizeBitsMax;

	//PublicKey_BNToEVPKEY(nid, pubKeyCompressed,pubKeyCompressedSize,osnma->evp_key);

	osnma->MSizeMax=13+MAX_KEY_SIZE_BYTES;
	M_DSSizeMax=osnma->MSizeMax+MAX_DS_SIZE_BYTES;
	M_DSSizeBitsMax=M_DSSizeMax*8;
	LBitsMax=(DSM_BLOCK_SIZE_BITS*OSNMA_DSM_MAX_NB)-M_DSSizeBitsMax;
	LBytesMax=LBitsMax/8;

	/* DSM-PKR related variables */
	osnma->pkrValid = FALSE_FLAG;
	osnma->pkrOam = FALSE_FLAG;
	osnma->mtIntermediateNodes = (unsigned char *)calloc(4*(MERKLE_TREE_NODE_SIZE/8),sizeof(unsigned char));
	osnma->pkrPublicKeyTmp = (unsigned char *)calloc(MAX_COMPRESSED_PUBLICKEY_SIZE,sizeof(unsigned char));
	osnma->pkrPublicKey = (unsigned char *)calloc(MAX_COMPRESSED_PUBLICKEY_SIZE,sizeof(unsigned char));
	osnma->pkrNpkLen = -1;
	osnma->pkrP2Len = -1;
	osnma->pkrP2 = (unsigned char *)calloc(DSM_BLOCK_SIZE_BITS/8,sizeof(unsigned char));
	/**/
	/*Merkle Tree Variables*/
	osnma->leaves=(unsigned char**)calloc(NUMBER_OF_LEAVES,sizeof(unsigned char*));
	for(index=0;index<NUMBER_OF_LEAVES;index++)
	{
		osnma->leaves[index]=(unsigned char*)calloc(ECDSA_COMPRESSED_KEY_LEN_P521/8,sizeof(unsigned char));
	}

	osnma->nodeMatrix=(unsigned char***)calloc(NUMBER_OF_LEVELS+1,sizeof(unsigned char**));
	for(indexRow=0;indexRow<(NUMBER_OF_LEVELS+1);indexRow++)
	{
		osnma->nodeMatrix[indexRow]=(unsigned char**)calloc(NUMBER_OF_LEAVES,sizeof(unsigned char*));
		for(indexColumn=0;indexColumn<NUMBER_OF_LEAVES;indexColumn++)
		{
			osnma->nodeMatrix[indexRow][indexColumn]=(unsigned char*)calloc(SHA256_DIGEST_LENGTH,sizeof(unsigned char));

		}
	}
	osnma->nodesConcatenated=(unsigned char*)calloc(2*SHA256_DIGEST_LENGTH,sizeof(unsigned char));

	osnma->mtRoot_leaf=(unsigned char*)calloc(SHA256_DIGEST_LENGTH+MAX_COMPRESSED_PUBLICKEY_SIZE+1,sizeof(unsigned char));
	osnma->mtRoot_leafHashed=(unsigned char*)calloc(SHA256_DIGEST_LENGTH,sizeof(unsigned char));
	osnma->P2Computed=(unsigned char*)calloc(DSM_BLOCK_SIZE_BITS/8,sizeof(unsigned char));

	/**/

	/*Allocate Memory for variables to verify the DSM-KROOT Digital Signature*/
	osnma->M=(unsigned char*)calloc(osnma->MSizeMax,sizeof(unsigned char));
	osnma->KROOT=(unsigned char*)calloc(MAX_KEY_SIZE_BYTES,sizeof(unsigned char));
	osnma->KROOTValidityFLag=FALSE_FLAG;
	osnma->newKROOT=(unsigned char*)calloc(MAX_KEY_SIZE_BYTES,sizeof(unsigned char));
	osnma->newKROOTValidityFLag=FALSE_FLAG;
	osnma->DS=(unsigned char*)calloc(MAX_DS_SIZE_BYTES,sizeof(unsigned char));
    osnma->DSToVerify=(unsigned char*)calloc(MAX_DS_SIZE_BYTES,sizeof(unsigned char));
	osnma->M_DS=(unsigned char*)calloc(M_DSSizeMax,sizeof(unsigned char));
	osnma->M_DSHashed=(unsigned char*)calloc(SHA256_DIGEST_LENGTH,sizeof(unsigned char));
	osnma->P1=(unsigned char*)calloc(LBytesMax,sizeof(unsigned char));
	osnma->P1Computed=(unsigned char*)calloc(LBytesMax,sizeof(unsigned char));

	/*Allocate Memory for variables to verify the TESLA KEY*/
	osnma->lastKeyVerified=(unsigned char*)calloc(MAX_KEY_SIZE_BYTES,sizeof(unsigned char));
	osnma->indexLastKeyVerified=-1;
	osnma->keyVerificationStatus=TO_BE_VALIDATED;
	osnma->currentKey=(unsigned char*)calloc(MAX_KEY_SIZE_BYTES,sizeof(unsigned char));
	osnma->referenceKey=(unsigned char*)calloc(MAX_KEY_SIZE_BYTES,sizeof(unsigned char));
	osnma->concatenatedInput=(unsigned char*)calloc(MAX_KEY_SIZE_BYTES + 4 + 6,sizeof(unsigned char));
	osnma->hashedInput=(unsigned char*)calloc(SHA256_DIGEST_LENGTH,sizeof(unsigned char));
	osnma->lastPrn1Key=(unsigned char**)calloc(MAX_NMACK_BLOCK,sizeof(unsigned char*));
    osnma->lastPrn1KeyTmp=(unsigned char**)calloc(MAX_NMACK_BLOCK,sizeof(unsigned char*));
	osnma->lastPrn19Key=(unsigned char**)calloc(MAX_NMACK_BLOCK,sizeof(unsigned char*));
    osnma->lastPrn19KeyTmp=(unsigned char**)calloc(MAX_NMACK_BLOCK,sizeof(unsigned char*));
	for(index=0;index<MAX_NMACK_BLOCK;index++)
	{
		osnma->lastPrn1Key[index]=(unsigned char*)calloc(MAX_KEY_SIZE_BYTES,sizeof(unsigned char));
		osnma->lastPrn19Key[index]=(unsigned char*)calloc(MAX_KEY_SIZE_BYTES,sizeof(unsigned char));
		osnma->indexLastPrn1Key[index]=-1;
		osnma->indexLastPrn19Key[index]=-1;
		osnma->towLastPrn1Key[index]=-1;
		osnma->towLastPrn19Key[index]=-1;
        //**
        osnma->lastPrn1KeyTmp[index]=(unsigned char*)calloc(MAX_KEY_SIZE_BYTES,sizeof(unsigned char));
		osnma->lastPrn19KeyTmp[index]=(unsigned char*)calloc(MAX_KEY_SIZE_BYTES,sizeof(unsigned char));
		osnma->indexLastPrn1KeyTmp[index]=-1;
		osnma->indexLastPrn19KeyTmp[index]=-1;
		osnma->towLastPrn1KeyTmp[index]=-1;
		osnma->towLastPrn19KeyTmp[index]=-1;
	}
    osnma->indexLastPrn19KeyToCheck=-1;
	//**
	osnma->newMacsAvailable = FALSE_FLAG;
	P4_max=8-(NMA_Status_Header_SizeBits+maxNavDataSizeBits)%8;
	osnma->messageSizeMax=3+GST_SFSize+(NMA_Status_Header_SizeBits+maxNavDataSizeBits+P4_max)/8;/*See pag 27 of ICD v1.1*/
	osnma->message=(unsigned char*)calloc(osnma->messageSizeMax,sizeof(unsigned char));
	osnma->messageForMacseqSizeMax=1+GST_SFSize+2*MAX_NUMBER_MFLEX_IN_MACK_BLOCK;/*See pag 28 of ICD v1.1*/
	osnma->messageForMacseq=(unsigned char*)calloc(osnma->messageForMacseqSizeMax,sizeof(unsigned char));
	osnma->messageHashed=(unsigned char*)calloc(SHA256_DIGEST_LENGTH,sizeof(unsigned char));
	osnma->tagSizeMax=ceil((double)MAX_MAC_SIZE_BITS/8);
	osnma->tag=(unsigned char*)calloc(osnma->tagSizeMax,sizeof(unsigned char));
	osnma->macSeqComputed=(unsigned char*)calloc(MACSEQ_SIZE_BYTES,sizeof(unsigned char));
	//**

	//**************************
	osnma->currentKeyThread=(unsigned char*)calloc(MAX_KEY_SIZE_BYTES,sizeof(unsigned char));
	osnma->referenceKeyThread=(unsigned char*)calloc(MAX_KEY_SIZE_BYTES,sizeof(unsigned char));
	osnma->keyThread=(unsigned char*)calloc(MAX_KEY_SIZE_BYTES,sizeof(unsigned char));
	osnma->lastKeyVerifiedThread=(unsigned char*)calloc(MAX_KEY_SIZE_BYTES,sizeof(unsigned char));
	osnma->indexLastKeyVerifiedThread=-1;
	osnma->keyVerificationStatusThread=TO_BE_VALIDATED;
	osnma->concatenatedInputThread=(unsigned char*)calloc(MAX_KEY_SIZE_BYTES + 4 + 6,sizeof(unsigned char));
	osnma->hashedInputThread=(unsigned char*)calloc(SHA256_DIGEST_LENGTH,sizeof(unsigned char));
	osnma->lastPrn1KeyThread=(unsigned char**)calloc(MAX_NMACK_BLOCK,sizeof(unsigned char*));
	osnma->lastPrn19KeyThread=(unsigned char**)calloc(MAX_NMACK_BLOCK,sizeof(unsigned char*));
    osnma->lastPrn1KeyThreadTmp=(unsigned char**)calloc(MAX_NMACK_BLOCK,sizeof(unsigned char*));
	osnma->lastPrn19KeyThreadTmp=(unsigned char**)calloc(MAX_NMACK_BLOCK,sizeof(unsigned char*));
	for(index=0;index<MAX_NMACK_BLOCK;index++)
	{
		osnma->lastPrn1KeyThread[index]=(unsigned char*)calloc(MAX_KEY_SIZE_BYTES,sizeof(unsigned char));
		osnma->lastPrn19KeyThread[index]=(unsigned char*)calloc(MAX_KEY_SIZE_BYTES,sizeof(unsigned char));
		osnma->indexLastPrn1KeyThread[index]=-1;
		osnma->indexLastPrn19KeyThread[index]=-1;
		osnma->towLastPrn1KeyThread[index]=-1;
		osnma->towLastPrn19KeyThread[index]=-1;
        //**
        osnma->lastPrn1KeyThreadTmp[index]=(unsigned char*)calloc(MAX_KEY_SIZE_BYTES,sizeof(unsigned char));
		osnma->lastPrn19KeyThreadTmp[index]=(unsigned char*)calloc(MAX_KEY_SIZE_BYTES,sizeof(unsigned char));
        //**
	}
	pthread_mutex_init(&osnma->TESLAKeyVerificationMutex, NULL);
	//**************************

}


void RestartTeslaKeyVericationVariables(TOsnmaData *osnma)
{
	int index;

	osnma->indexLastKeyVerified=-1;
	osnma->keyVerificationStatus=TO_BE_VALIDATED;
	for(index=0;index<MAX_NMACK_BLOCK;index++)
	{
		osnma->indexLastPrn1Key[index]=-1;
		osnma->indexLastPrn19Key[index]=-1;
		osnma->towLastPrn1Key[index]=-1;
		osnma->towLastPrn19Key[index]=-1;
		osnma->indexLastPrn1KeyTmp[index]=-1;
		osnma->indexLastPrn19KeyTmp[index]=-1;
		osnma->towLastPrn1KeyTmp[index]=-1;
		osnma->towLastPrn19KeyTmp[index]=-1;
	}
	osnma->indexLastPrn19KeyToCheck=-1;
	osnma->newMacsAvailable = FALSE_FLAG;
	//**
	osnma->indexLastKeyVerifiedThread=-1;
	osnma->keyVerificationStatusThread=TO_BE_VALIDATED;
	for(index=0;index<MAX_NMACK_BLOCK;index++)
	{
		osnma->indexLastPrn1KeyThread[index]=-1;
		osnma->indexLastPrn19KeyThread[index]=-1;
		osnma->towLastPrn1KeyThread[index]=-1;
		osnma->towLastPrn19KeyThread[index]=-1;
	}
}


void DeinitOsnmaData(TOsnmaData *osnma)
{
	int index,indexRow,indexColumn;

	DeinitOsnmaParameters(&(osnma->osnmaPars));
	DeinitOsnmaParameters(&(osnma->osnmaParsNew));
	DeleteMackList(&(osnma->mackList));
	DeinitDsmData(&(osnma->dsmData));
	if(osnma->parsLogFile!=NULL)
	{
		fclose(osnma->parsLogFile);
		osnma->parsLogFile=NULL;
	}
	if(osnma->macLogFile!=NULL)
	{
		fclose(osnma->macLogFile);
		osnma->macLogFile=NULL;
	}
	if(osnma->macseqLogFile!=NULL)
	{
		fclose(osnma->macseqLogFile);
		osnma->macseqLogFile=NULL;
	}
	if(osnma->teslaChainFile!=NULL)
	{
		fclose(osnma->teslaChainFile);
		osnma->teslaChainFile=NULL;
	}
	if(osnma->dsmLogFile!=NULL)
	{
		fclose(osnma->dsmLogFile);
		osnma->dsmLogFile=NULL;
	}
	if(osnma->osnmaStatusLogFile!=NULL)
	{
		fclose(osnma->osnmaStatusLogFile);
		osnma->osnmaStatusLogFile=NULL;
	}
	if(osnma->navdataLogFile!=NULL)
	{
		fclose(osnma->navdataLogFile);
		osnma->navdataLogFile=NULL;
	}
	if(osnma->validationLogFile!=NULL)
	{
		fclose(osnma->validationLogFile);
		osnma->validationLogFile=NULL;
	}
	if(osnma->authLogFile!=NULL)
	{
		fclose(osnma->authLogFile);
		osnma->authLogFile=NULL;
	}
	if(osnma->simpleLogFile!=NULL)
	{
		fclose(osnma->simpleLogFile);
		osnma->simpleLogFile=NULL;
	}
	if(osnma->simpleLogBinFile!=NULL)
	{
		fclose(osnma->simpleLogBinFile);
		osnma->simpleLogBinFile=NULL;
	}
	//**
	#ifdef OSNMA_PROFILING
	ReportProfilingData(&(osnma->dsmKrootProfiling));
	ReportProfilingData(&(osnma->P1Verification));
	ReportProfilingData(&(osnma->merkleTreeVerification));
	ReportProfilingData(&(osnma->P2Verification));
	ReportProfilingData(&(osnma->pubPEMToEVPKey));
	ReportProfilingData(&(osnma->teslaProfiling));
	ReportProfilingData(&(osnma->oneStepOfTeslaChainProfiling));
	ReportProfilingData(&(osnma->allMacsProfiling));
	ReportProfilingData(&(osnma->macProfiling));
	ReportProfilingData(&(osnma->macSeqProfiling));
	ReportProfilingData(&(osnma->saveMackInMackList));
	ReportProfilingData(&(osnma->saveMackInMackListExcludingTESLA));
	ReportProfilingData(&(osnma->saveNavdataInMackList));
	ReportProfilingData(&(osnma->saveDsmBlock));
	ReportProfilingData(&(osnma->extractAndCheckBitsForMACVerification));
	ReportProfilingData(&(osnma->findKeyForMACVerification));
	ReportProfilingData(&(osnma->mainLoop));
	#endif
	//**
	/* DSM-PKR related variables */
	free(osnma->mtIntermediateNodes);
	free(osnma->pkrPublicKeyTmp);
	free(osnma->pkrPublicKey);
	free(osnma->pkrP2);
	for(index=0;index<NUMBER_OF_LEAVES;index++)
	{
		free(osnma->leaves[index]);
	}
	free(osnma->leaves);
	for(indexRow=0;indexRow<(NUMBER_OF_LEVELS+1);indexRow++)
	{
		for(indexColumn=0;indexColumn<NUMBER_OF_LEAVES;indexColumn++)
		{
			free(osnma->nodeMatrix[indexRow][indexColumn]);

		}
		free(osnma->nodeMatrix[indexRow]);
	}
	free(osnma->nodeMatrix);
	free(osnma->nodesConcatenated);
	free(osnma->mtRoot_leaf);
	free(osnma->mtRoot_leafHashed);
	free(osnma->P2Computed);

	/**/
	EVP_PKEY_free(osnma->evp_key);
	free(osnma->M);
	free(osnma->KROOT);
	free(osnma->newKROOT);
	free(osnma->DS);
    free(osnma->DSToVerify);
	free(osnma->M_DS);
	free(osnma->M_DSHashed);
	free(osnma->P1);
	free(osnma->P1Computed);
	/**/
	free(osnma->lastKeyVerified);
	free(osnma->currentKey);
	free(osnma->referenceKey);
	free(osnma->concatenatedInput);
	free(osnma->hashedInput);
	for(index=0;index<MAX_NMACK_BLOCK;index++)
	{
		free(osnma->lastPrn1Key[index]);
		free(osnma->lastPrn19Key[index]);
        free(osnma->lastPrn1KeyTmp[index]);
		free(osnma->lastPrn19KeyTmp[index]);
	}
	free(osnma->lastPrn1Key);
	free(osnma->lastPrn19Key);
    free(osnma->lastPrn1KeyTmp);
	free(osnma->lastPrn19KeyTmp);
	//**
	free(osnma->message);
	free(osnma->messageForMacseq);
	free(osnma->messageHashed);
	free(osnma->tag);
	free(osnma->macSeqComputed);

	//*************************
	free(osnma->currentKeyThread);
	free(osnma->referenceKeyThread);
	free(osnma->keyThread);
	free(osnma->lastKeyVerifiedThread);
	free(osnma->concatenatedInputThread);
	free(osnma->hashedInputThread);
	for(index=0;index<MAX_NMACK_BLOCK;index++)
	{
		free(osnma->lastPrn1KeyThread[index]);
		free(osnma->lastPrn19KeyThread[index]);
        free(osnma->lastPrn1KeyThreadTmp[index]);
		free(osnma->lastPrn19KeyThreadTmp[index]);
	}
	free(osnma->lastPrn1KeyThread);
	free(osnma->lastPrn19KeyThread);
    free(osnma->lastPrn1KeyThreadTmp);
	free(osnma->lastPrn19KeyThreadTmp);
	pthread_mutex_destroy(&osnma->TESLAKeyVerificationMutex);

}


void InitChannelOsnmaVariables(TChannel *ch, TChannelOsnmaVariables *osnma)
{
	osnma->satType = ch->satType;
	osnma->satSignal = ch->satSignal;
	osnma->chId = ch->chIndex;
	osnma->prn = ch->satId;
	osnma->wnSubframeStart = -1; /* Not valid */
	osnma->towSubframeStart = -1; /* Not valid */
	/**/
	osnma->nmaStatus = NMA_VALUE_NOT_AVAILABLE;
	osnma->cpks = RESERVED_0;
	osnma->cidInForce = -1;
	osnma->rxOsnmaStatus = RX_OSNMA_NOT_PRESENT;
	/**/
	osnma->crossAuth = FALSE_FLAG;
	osnma->chOsnmaStatus = CH_OSNMA_IDLE;
	osnma->chDsmIndex = 0;
	osnma->chMackIndex = 0;
	osnma->chMackOffset = -1; /* Not valid */
	/**/
	osnma->dsmStatusPtr = &(ch->dsmStatus);
	osnma->keyStatusPtr = &(ch->keyStatus);
	osnma->msgStatusPtr = &(ch->msgStatus);
	osnma->macseqStatusPtr = &(ch->macseqStatus);
	sprintf(osnma->chOsnmaStatusLog," ");
	osnma->chOsnmaStatusLogColor = NC_COLOR_SCHEME_NO_OSNMA;
}


void UpdateChannelOsnmaVariables(TOsnmaData *rxOsnma,TOsnmaAuthentication *auth,TChannelOsnmaVariables *chOsnma,int wn,int tow,int wordType,unsigned char *evenPart,unsigned char *oddPart,unsigned char *rsvd1)
{
	int subfIndex;
	int idx;
	int dsmid;
	int bid;
	int wordOffset; // seconds from the start time of the subframe, initialized in the next line
	int towSubframeStart = GetGalE1bSubframeStartTime(tow,&wordOffset); // [s]
	char *charPtr;
	TNmaStatus nmaStatus;
	int allZeros;
	//**
	TDsmData *dsmData = &(rxOsnma->dsmData);
	TOsnmaParameters *pars = &(rxOsnma->osnmaPars);
	TOsnmaParameters *parsNew = &(rxOsnma->osnmaParsNew);
	TOsnmaParameters *parsTmp;
	char DSVerifyDsmData=-1,P1VerifyDsmData=-1,DSVerifyDsmArray=-1,P1VerifyDsmArray=-1;
	char PKVerifyDsmData=-1, PKVerifyDsmArray=-1, P2VerifyDsmData=-1,P2VerifyDsmArray=-1;
	char dsmDataVerified=FALSE_FLAG;
	char dsmArrayDataVerified=FALSE_FLAG;
	TDsmArray *dsmElement;
	//**

	/* sanity check to avoid synchronization errors! */
	if((chOsnma->chOsnmaStatus==CH_OSNMA_OPERATIONAL) || (chOsnma->chOsnmaStatus==CH_OSNMA_WAIT_VALID_NMA_HEADER))
	{
		// in these state the synchronization should be ok.
		// check DSM message synchronization
		// (check with time and not page type because, due to I/NAV2, the order of pages can change!)
		if(wordOffset!=(chOsnma->chDsmIndex*GAL_SECONDS_PER_PAGE))
		{
			RestartChannelOsnmaVariables(rxOsnma,chOsnma);
		}
		// check MACK synchronization
		if((wordOffset==chOsnma->chMackOffset) && (chOsnma->chMackIndex!=0))
		{
			RestartChannelOsnmaVariables(rxOsnma,chOsnma);
		}
	}

	/*  debug of the OSNMA channel state machine */
	if((tow>0) && (rxOsnma->osnmaStatusLogFile!=NULL))
	{
		fprintf(rxOsnma->osnmaStatusLogFile,"%2d  ",chOsnma->prn);
		fprintf(rxOsnma->osnmaStatusLogFile,"%6d  ",(int)floor(tow));
		fprintf(rxOsnma->osnmaStatusLogFile,"%2d  ",(int)chOsnma->chOsnmaStatus);
		fprintf(rxOsnma->osnmaStatusLogFile,"%2d  ",chOsnma->crossAuth);
		fprintf(rxOsnma->osnmaStatusLogFile,"%2d  ",chOsnma->chDsmIndex);
		fprintf(rxOsnma->osnmaStatusLogFile,"%2d         ",chOsnma->chMackIndex);
		fprintf(rxOsnma->osnmaStatusLogFile,"%2d    ",wordType);
		fprintf(rxOsnma->osnmaStatusLogFile,"%4u ",rsvd1[0]);
		fprintf(rxOsnma->osnmaStatusLogFile,"%4u ",rsvd1[1]);
		fprintf(rxOsnma->osnmaStatusLogFile,"%4u ",rsvd1[2]);
		fprintf(rxOsnma->osnmaStatusLogFile,"%4u ",rsvd1[3]);
		fprintf(rxOsnma->osnmaStatusLogFile,"%4u ",rsvd1[4]);
		fprintf(rxOsnma->osnmaStatusLogFile,"         %2d ",chOsnma->rxOsnmaStatus);
		fprintf(rxOsnma->osnmaStatusLogFile,"\n");
	}

	/* here it manages the OSNMA status and then all transients related to renewals/revocations */
	if(wordType==2)
	{
		// NMA status and other important info are in DSM byte in the word 2
		chOsnma->nmaStatus = (TNmaStatus)((rsvd1[0]>>6) & 0x03);
		chOsnma->cpks = (TChainAndPublicKeyStatus)((rsvd1[0]>>1) & 0x07);
		chOsnma->cidInForce = (rsvd1[0]>>4) & 0x03;
		// update (when needed) nmaHeader in the parameters structure
		if((pars->parametersValid==TRUE_FLAG) && (pars->nmaHeader!=rsvd1[0]) && (rsvd1[0]!=0x00))
		{
			pars->nmaHeader = rsvd1[0];
		}

		/* Now manage the state machine that manages renewals/revocation */
		switch(chOsnma->rxOsnmaStatus)
		{
			case RX_OSNMA_NOT_PRESENT:
				if((chOsnma->nmaStatus!=NMA_VALUE_NOT_AVAILABLE) && (chOsnma->cpks!=RESERVED_0))
				{
					// stays in the default state until some valid values are found
					chOsnma->rxOsnmaStatus = RX_OSNMA_OP_NOMINAL;
				}
				break;
			case RX_OSNMA_OP_NOMINAL:
				if((chOsnma->nmaStatus==NMA_DO_NOT_USE) && ((chOsnma->cpks==CREV) || (chOsnma->cpks==PKREV)))
				{
					if((chOsnma->cpks==CREV) && (rxOsnma->rxOsnmaStatusLastPrint!=RX_OSNMA_OP_NOMINAL))
					{
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"OSNMA Status: TESLA chain revocation found!\n");
						rxOsnma->rxOsnmaStatusLastPrint = RX_OSNMA_OP_NOMINAL;
					}
					else if((chOsnma->cpks==PKREV) && (rxOsnma->rxOsnmaStatusLastPrint!=RX_OSNMA_OP_NOMINAL))
					{
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"OSNMA Status: Public Key revocation found!\n");
						rxOsnma->rxOsnmaStatusLastPrint = RX_OSNMA_OP_NOMINAL;
					}
					// a revocation occurred!, clean all needed variables
					RestartChannelOsnmaVariables(rxOsnma,chOsnma);
					if(rxOsnma->revocationStarted==FALSE_FLAG)
					{
						// the receiver variables are cleared only once
						pars->parametersValid = FALSE_FLAG;
						parsNew->parametersValid = FALSE_FLAG;
						DeleteMackList(&(rxOsnma->mackList));
						ClearDsmValidityAll(&(rxOsnma->dsmData));
						if(chOsnma->cpks==PKREV)
						{
							// discard the public key only when the Public Key is revoked
							rxOsnma->evp_keyPkid = INVALID_VALUE;
							rxOsnma->evp_keyValid = FALSE_FLAG;
						}
						rxOsnma->KROOTValidityFLag = FALSE_FLAG;
						RestartTeslaKeyVericationVariables(rxOsnma);
						// discard old validations
						ResetOsnmaAuthenticationStruct(auth);
						//
						rxOsnma->revocationStarted = TRUE_FLAG;
					}
					// update the state
					chOsnma->rxOsnmaStatus = RX_OSNMA_DU_WAIT_OP;
				}
				else if(rxOsnma->pkrOam==TRUE_FLAG)
				{
					// OAM behavior from dataset TV 233
					chOsnma->rxOsnmaStatus = RX_OSNMA_EMERGENCY;
					if(rxOsnma->rxOsnmaStatusLastPrint!=RX_OSNMA_OP_NOMINAL)
					{
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"OSNMA Status: Emergency Message found!\n");
						rxOsnma->rxOsnmaStatusLastPrint = RX_OSNMA_OP_NOMINAL;
					}
				}
				else if((chOsnma->nmaStatus!=NMA_DO_NOT_USE) && (chOsnma->cpks==EOC))
				{
					if(rxOsnma->rxOsnmaStatusLastPrint!=RX_OSNMA_OP_NOMINAL)
					{
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"OSNMA Status: TESLA Chain renewal (EOC) found!\n");
						rxOsnma->rxOsnmaStatusLastPrint = RX_OSNMA_OP_NOMINAL;
					}
					// End Of TESLA Chain (TESLA chain renewal)
					chOsnma->rxOsnmaStatus = RX_OSNMA_EOC_WAIT_NOMINAL;
				}
				else if((chOsnma->nmaStatus!=NMA_DO_NOT_USE) && (chOsnma->cpks==NPK))
				{
					if(rxOsnma->rxOsnmaStatusLastPrint!=RX_OSNMA_OP_NOMINAL)
					{
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"OSNMA Status: Public Key renewal (NPK) found!\n");
						rxOsnma->rxOsnmaStatusLastPrint = RX_OSNMA_OP_NOMINAL;
					}
					// New Public Key (Public Key renewal)
					chOsnma->rxOsnmaStatus = RX_OSNMA_NPK_WAIT_NOMINAL;
				}
				break;
			case RX_OSNMA_DU_WAIT_OP:
				// move away from this state only when nmaStatus is Operational again
				if((chOsnma->nmaStatus==NMA_OPERATIONAL) || (chOsnma->nmaStatus==NMA_TEST))
				{
					if(rxOsnma->rxOsnmaStatusLastPrint!=RX_OSNMA_DU_WAIT_OP)
					{
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"OSNMA Status: A revocation is finished!\n");
						rxOsnma->rxOsnmaStatusLastPrint = RX_OSNMA_DU_WAIT_OP;
					}
					// if the new parameters are not available, copy them
					if((pars->parametersValid==FALSE_FLAG) && (parsNew->parametersValid==TRUE_FLAG))
					{
						CopyOsnmaParameters(parsNew,pars,TRUE_FLAG);
					}
					// delete useless data
					parsNew->parametersValid = FALSE_FLAG;
					// go to the nominal state
					chOsnma->rxOsnmaStatus = RX_OSNMA_OP_NOMINAL;
					rxOsnma->revocationStarted = FALSE_FLAG;
				}
				// move away from this state when an OAM is found
				if(rxOsnma->pkrOam==TRUE_FLAG)
				{
					// OAM behavior described in a mail from Sophie Damy (11/01/2021)
					chOsnma->rxOsnmaStatus = RX_OSNMA_EMERGENCY;
					if(rxOsnma->rxOsnmaStatusLastPrint!=RX_OSNMA_DU_WAIT_OP)
					{
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"OSNMA Status: Emergency Message found!\n");
						rxOsnma->rxOsnmaStatusLastPrint = RX_OSNMA_DU_WAIT_OP;
					}
				}
				break;
			case RX_OSNMA_EOC_WAIT_NOMINAL:
				// wait CPKS nominal
				if((chOsnma->cpks==NOMINAL) && (rxOsnma->newKROOTValidityFLag==TRUE_FLAG))
				{
					/* exit from this state only when the new Root key is available */
					if(rxOsnma->rxOsnmaStatusLastPrint!=RX_OSNMA_EOC_WAIT_NOMINAL)
					{
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"OSNMA Status: TESLA chain renewal is finished!\n");
						rxOsnma->rxOsnmaStatusLastPrint = RX_OSNMA_EOC_WAIT_NOMINAL;
					}
					// save new data
					CopyOsnmaParameters(parsNew,pars,TRUE_FLAG);
					memcpy(rxOsnma->KROOT, rxOsnma->newKROOT, pars->keySizeBytes);
					memcpy(rxOsnma->lastKeyVerified, rxOsnma->newKROOT, pars->keySizeBytes);

					#ifdef OSNMA_DSM_KROOT_DEBUG
					{
						FILE *rootLogFile = fopen("osnmaTeslaRoot.txt","a");
						int rootLogIndex;
						for(rootLogIndex=0;rootLogIndex<pars->keySizeBytes;rootLogIndex++)
						{
							fprintf(rootLogFile,"%u ",rxOsnma->KROOT[rootLogIndex]);
						}
						fprintf(rootLogFile,"\n");
						fprintf(rootLogFile,"%d\n\n",pars->krootTow);
						fclose(rootLogFile);
					}
					#endif

					RestartTeslaKeyVericationVariables(rxOsnma);
					rxOsnma->indexLastKeyVerified=0; // overwrite with 1 because the Root key is available!
					rxOsnma->KROOTValidityFLag=rxOsnma->newKROOTValidityFLag;
					DeleteAllKeysAndMacsFromMackList(rxOsnma->mackList); 
					// delete useless data
					parsNew->parametersValid = FALSE_FLAG;
					rxOsnma->newKROOTValidityFLag = FALSE_FLAG;
					// restart validations
					ResetOsnmaAuthenticationStruct(auth);
					// go to the nominal state
					chOsnma->rxOsnmaStatus = RX_OSNMA_OP_NOMINAL;
				}
				break;
			case RX_OSNMA_NPK_WAIT_NOMINAL:
				// wait CPKS nominal
				if(chOsnma->cpks==NOMINAL)
				{
					if(rxOsnma->rxOsnmaStatusLastPrint!=RX_OSNMA_NPK_WAIT_NOMINAL)
					{
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"OSNMA Status: Public Key renewal is finished!\n");
						rxOsnma->rxOsnmaStatusLastPrint = RX_OSNMA_NPK_WAIT_NOMINAL;
					}
					// delete useless data
					parsNew->parametersValid = FALSE_FLAG;
					// restart validations
					ResetOsnmaAuthenticationStruct(auth);
					// go to the nominal state
					chOsnma->rxOsnmaStatus = RX_OSNMA_OP_NOMINAL;
				}
				break;
			case RX_OSNMA_EMERGENCY:
				// OSNMA is stopped
				// OAM behavior described in a mail from Sophie Damy (11/01/2021)
				return;
				/* this code enabled to continue the elaboration of OSNMA after an OAM */
				//if((chOsnma->nmaStatus==NMA_OPERATIONAL) || (chOsnma->nmaStatus==NMA_TEST))
				//{
				//	if(rxOsnma->rxOsnmaStatusLastPrint!=RX_OSNMA_EMERGENCY)
				//	{
				//		SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"OSNMA Status: OAM reception is finished!\n");
				//		rxOsnma->rxOsnmaStatusLastPrint = RX_OSNMA_EMERGENCY;
				//	}
				//	rxOsnma->pkrOam = FALSE_FLAG;
				//	chOsnma->rxOsnmaStatus = RX_OSNMA_OP_NOMINAL;
				//}
				break;
			default:
				break;
		}
	}


	/* this state machine stays in CH_OSNMA_IDLE until (at least) one satellite transmits
	 * some bit different from zero in the NMA header */
	if((chOsnma->chOsnmaStatus==CH_OSNMA_IDLE) && (rxOsnma->osnmaTransmitted==TRUE_FLAG))
	{
		chOsnma->chOsnmaStatus=CH_OSNMA_WAIT_SUBFRAME_START;
	}
	/* this state machine is in states CH_OSNMA_WAIT_SUBFRAME_START and CH_OSNMA_WAIT_DSMKROOT_BID0
	 * only when OSNMA parameters are not available */
	if((chOsnma->chOsnmaStatus==CH_OSNMA_WAIT_SUBFRAME_START) && (pars->parametersValid==TRUE_FLAG))
	{
		chOsnma->chOsnmaStatus = CH_OSNMA_SYNC_MACK_DSM; // still need to synchronize MACK and DSM
	}
	else if((chOsnma->chOsnmaStatus==CH_OSNMA_WAIT_DSMKROOT_BID0) && (pars->parametersValid==TRUE_FLAG))
	{
		chOsnma->chOsnmaStatus = CH_OSNMA_SYNC_MACK; // still need to synchronize MACK
	}
	else if((chOsnma->chOsnmaStatus==CH_OSNMA_WAIT_DSMKROOT_BID0) && (chOsnma->rxOsnmaStatus==RX_OSNMA_DU_WAIT_OP) && (parsNew->parametersValid==TRUE_FLAG))
	{
		chOsnma->chOsnmaStatus = CH_OSNMA_SYNC_MACK; // still need to synchronize MACK
	}
	/* set chMackOffset if missing */
	#ifdef MACK_OFFSET_ICD2_DEFINITION
	if((pars->parametersValid==TRUE_FLAG) && (chOsnma->chMackOffset<0.0))
	{
		chOsnma->chMackOffset = GetMackOffsetSeconds(chOsnma->prn,pars->mackOffset,pars->nmack,pars->ns);
	}
	if((chOsnma->rxOsnmaStatus==RX_OSNMA_DU_WAIT_OP) && (parsNew->parametersValid==TRUE_FLAG) && (chOsnma->chMackOffset<0.0))
	{
		chOsnma->chMackOffset = GetMackOffsetSeconds(chOsnma->prn,parsNew->mackOffset,parsNew->nmack,parsNew->ns);
	}
	#else
	if((pars->parametersValid==TRUE_FLAG) && (chOsnma->chMackOffset<0.0))
	{
		chOsnma->chMackOffset = GetMackOffsetSeconds(chOsnma->prn,pars->mackOffset,pars->nmack);
	}
	if((chOsnma->rxOsnmaStatus==RX_OSNMA_DU_WAIT_OP) && (parsNew->parametersValid==TRUE_FLAG) && (chOsnma->chMackOffset<0.0))
	{
		chOsnma->chMackOffset = GetMackOffsetSeconds(chOsnma->prn,parsNew->mackOffset,parsNew->nmack);
	}
	#endif

	/* copy reserved bits */
	chOsnma->chDsm[chOsnma->chDsmIndex] = rsvd1[0];
	chOsnma->chMack[chOsnma->chMackIndex] = rsvd1[1];
	chOsnma->chMack[chOsnma->chMackIndex+1] = rsvd1[2];
	chOsnma->chMack[chOsnma->chMackIndex+2] = rsvd1[3];
	chOsnma->chMack[chOsnma->chMackIndex+3] = rsvd1[4];
	if((rsvd1[0]==0x00) && (rsvd1[1]==0x00) && (rsvd1[2]==0x00) && (rsvd1[3]==0x00) && (rsvd1[4]==0x00) && (rsvd1[5]==0x00))
	{
		allZeros = TRUE_FLAG;
	}
	else
	{
		allZeros = FALSE_FLAG;
	}

	/* copy navigation message bits */
	// subframe is aligned as DSM: use the same index adapted to the different size of variables
	chOsnma->chWordTypes[chOsnma->chDsmIndex] = wordType;
	subfIndex = chOsnma->chDsmIndex*GAL_DECODED_PAGE_PART_LENGTH_BYTES*2;
	for(idx=0;idx<GAL_DECODED_PAGE_PART_LENGTH_BYTES;idx++)
	{
		// only 15 bytes per variable: use simd to copy these data is useless!
		chOsnma->chSubframe[subfIndex+idx] = evenPart[idx];
		chOsnma->chSubframe[subfIndex+GAL_DECODED_PAGE_PART_LENGTH_BYTES+idx] = oddPart[idx];
	}

	/* state machine */
	switch(chOsnma->chOsnmaStatus)
	{
	case CH_OSNMA_IDLE:
		if(chOsnma->chDsm[0]!=0x00)
		{
			/* Bits for OSNMA are present, leave the CH_OSNMA_IDLE state and start synchronization */
			rxOsnma->osnmaTransmitted = TRUE_FLAG;
			chOsnma->crossAuth = FALSE_FLAG;
			if(wordType==2)
			{
				/* Start of DSM Block */
				chOsnma->chDsmIndex = 1;
				chOsnma->chOsnmaStatus = CH_OSNMA_WAIT_DSMKROOT_BID0;
			}
			else
			{
				/* wordType!=2, so the start of the subframe must be found yet! */
				chOsnma->chOsnmaStatus = CH_OSNMA_WAIT_SUBFRAME_START;
			}
		}
		break;
		/* transient when OSNMA parameters are not valid */
	case CH_OSNMA_WAIT_SUBFRAME_START:
		if(wordType==2)
		{
			/* Start of DSM Block */
			chOsnma->chDsmIndex = 1;
			chOsnma->crossAuth = (chOsnma->chDsm[0]==0x00) ? TRUE_FLAG : FALSE_FLAG;
			chOsnma->chOsnmaStatus = CH_OSNMA_WAIT_DSMKROOT_BID0;
		}
		break;
	case CH_OSNMA_WAIT_DSMKROOT_BID0:
		chOsnma->chDsmIndex++;
		// check for cross-authentication
		if(wordType==2)
		{
			/* check cross */
			chOsnma->crossAuth = (chOsnma->chDsm[0]==0x00) ? TRUE_FLAG : FALSE_FLAG;
		}
		else if(allZeros==TRUE_FLAG)
		{
			chOsnma->crossAuth = TRUE_FLAG;
		}
		// check if OSNMA parameters are available
		if((chOsnma->crossAuth==FALSE_FLAG) && (chOsnma->chDsmIndex==GAL_PAGES_PER_SUBFRAME))
		{
			/* Analyze DSM Header */
			dsmid = (chOsnma->chDsm[1]>>4) & 0x0f;
			bid = chOsnma->chDsm[1] & 0x0f;
			if((GetDsmTypeFromDsmId(dsmid)==DSM_KROOT) && (bid==0) && (chOsnma->chDsm[0]!=0x00))
			{
				// load new parameters only when they are not available
				// do not try to get them from a cross-authenticated PRN
				GetOsnmaParameters(parsNew,chOsnma->chDsm,TRUE_FLAG,rxOsnma->parsLogFile);
				if((parsNew->parametersValid==TRUE_FLAG) && (parsNew->chainId==chOsnma->cidInForce))
				{
					// load the parameters only when they refer to the TESLA chain in force
					// useful when the receiver is powered on during a chain renewal
					chOsnma->chOsnmaStatus = CH_OSNMA_SYNC_MACK;
					if(pars->parametersValid==FALSE_FLAG)
					{
						CopyOsnmaParameters(parsNew,pars,TRUE_FLAG);
						// manage a possible change of PK
						if((rxOsnma->pkrValid==TRUE_FLAG) && (rxOsnma->evp_keyValid==FALSE_FLAG) && (pars->pkId==pars->dsmPkrNpkId))
						{
							SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The ECDSA Public Key is replaced (new PKID %d)\n",parsNew->pkId);
							PublicKey_PkrToEVPKEY(pars->dsmPkrNpkt, rxOsnma->pkrPublicKey,(rxOsnma->pkrNpkLen/8),rxOsnma->evp_key);
							rxOsnma->evp_keyPkid = pars->dsmPkrNpkId;
							rxOsnma->evp_keyValid=TRUE_FLAG;

							switch(pars->dsmPkrNpkt)
							{
							case ECDSA_P224:
								rxOsnma->function = EVP_sha224();
								pars->DSSizeBits = 448;
								break;
							case ECDSA_P256:
								rxOsnma->function = EVP_sha256();
								pars->DSSizeBits = 512;
								break;
							case ECDSA_P384:
								rxOsnma->function = EVP_sha384();
								pars->DSSizeBits = 768;
								break;
							case ECDSA_P521:
								rxOsnma->function = EVP_sha512();
								#ifdef PATCH_JRC_GENERATOR_DSM
								pars->DSSizeBits = 1042;
								#else
								pars->DSSizeBits = 1056;
								#endif
								break;
							default:
								break;
							}
							pars->DSSizeBytes=pars->DSSizeBits/8;
							#ifdef PATCH_JRC_GENERATOR_DSM
							if(pars->dsmPkrNpkt==ECDSA_P521)
							{
								pars->DSSizeBytes= 132;
							}
							#endif
							// update parameters that depend on the PK/DS length
							pars->M_DSSizeBits=pars->MSize*8+pars->DSSizeBits;
							pars->M_DSSizeBytes=ceil((double)pars->M_DSSizeBits/8);
							pars->LBits=(104*pars->dsmKrootNb)-pars->M_DSSizeBits;
							pars->LBytes=ceil((double)pars->LBits/8);
						}
					}
					#ifdef MACK_OFFSET_ICD2_DEFINITION
					chOsnma->chMackOffset = GetMackOffsetSeconds(chOsnma->prn,pars->mackOffset,pars->nmack,pars->ns);
					#else
					chOsnma->chMackOffset = GetMackOffsetSeconds(chOsnma->prn,pars->mackOffset,pars->nmack);
					#endif
					/* do not restart index, to save the current block of DSM-KROOT */
				}
			}
			else
			{
				/* it's not bid==0 or it is cross-authenticated, restart index */
				chOsnma->chDsmIndex = 0;
			}
		}
		break;
		/* when OSNMA parameters are valid */
	case CH_OSNMA_SYNC_MACK_DSM:
		if(wordOffset==chOsnma->chMackOffset)
		{
			/* MACK section starts here! */
			chOsnma->chMackIndex = MACK_PER_WORD_SIZE;
		}
		if(wordType==2)
		{
			/* Start of DSM Block */
			chOsnma->chDsmIndex = 1;
			/* check cross */
			chOsnma->crossAuth = (chOsnma->chDsm[0]==0x00) ? TRUE_FLAG : FALSE_FLAG;
		}
		else if(allZeros==TRUE_FLAG)
		{
			chOsnma->crossAuth = TRUE_FLAG;
		}
		/* next state: synch still needed? */
		if((chOsnma->chDsmIndex==1) && (chOsnma->chMackIndex==MACK_PER_WORD_SIZE))
		{
			chOsnma->chOsnmaStatus = (chOsnma->crossAuth==TRUE_FLAG) ? CH_OSNMA_WAIT_VALID_NMA_HEADER : CH_OSNMA_OPERATIONAL;
		}
		else if(chOsnma->chDsmIndex==1)
		{
			chOsnma->chOsnmaStatus = CH_OSNMA_SYNC_MACK;
		}
		else if(chOsnma->chMackIndex==MACK_PER_WORD_SIZE)
		{
			chOsnma->chOsnmaStatus = CH_OSNMA_SYNC_DSM;
		}
		break;
	case CH_OSNMA_SYNC_MACK:
		chOsnma->chDsmIndex++; // already synchronized
		if(wordType==2)
		{
			/* check cross */
			chOsnma->crossAuth = (chOsnma->chDsm[0]==0x00) ? TRUE_FLAG : FALSE_FLAG;
		}
		else if(allZeros==TRUE_FLAG)
		{
			chOsnma->crossAuth = TRUE_FLAG;
		}
		if(wordOffset==chOsnma->chMackOffset)
		{
			/* MACK section starts here! */
			chOsnma->chMackIndex = MACK_PER_WORD_SIZE;
			chOsnma->chOsnmaStatus = (chOsnma->crossAuth==TRUE_FLAG) ? CH_OSNMA_WAIT_VALID_NMA_HEADER : CH_OSNMA_OPERATIONAL;
		}
		break;
	case CH_OSNMA_SYNC_DSM:
		chOsnma->chMackIndex += MACK_PER_WORD_SIZE;  // already synchronized
		if(wordType==2)
		{
			/* NMA Header */
			chOsnma->chDsmIndex = 1;
			chOsnma->crossAuth = (chOsnma->chDsm[0]==0x00) ? TRUE_FLAG : FALSE_FLAG;
			chOsnma->chOsnmaStatus = (chOsnma->crossAuth==TRUE_FLAG) ? CH_OSNMA_WAIT_VALID_NMA_HEADER : CH_OSNMA_OPERATIONAL;
		}
		else if(allZeros==TRUE_FLAG)
		{
			chOsnma->crossAuth = TRUE_FLAG;
		}
		break;
	case CH_OSNMA_OPERATIONAL:
		chOsnma->chDsmIndex++;
		chOsnma->chMackIndex += MACK_PER_WORD_SIZE;
		/* sometimes, during operations, OSNMA could become not available for the current channel */
		if((chOsnma->crossAuth==FALSE_FLAG) && (wordType==2) && (chOsnma->chDsm[0]==0x00))
		{
			chOsnma->crossAuth = TRUE_FLAG;
			chOsnma->chOsnmaStatus = CH_OSNMA_WAIT_VALID_NMA_HEADER;
		}
		/* sometimes this switch-off of the OSNMA bits seems not aligned with the subframe
		 * and can happen in a word different from the 2! */
		if((chOsnma->crossAuth==FALSE_FLAG) && (allZeros==TRUE_FLAG))
		{
			chOsnma->crossAuth = TRUE_FLAG;
			chOsnma->chOsnmaStatus = CH_OSNMA_WAIT_VALID_NMA_HEADER;
		}
		/* maybe, OSNMA is made available for the current channel even if it was not so during the initial transient
		 * correct the cross-authentication flag and restart the state machine */
		if((chOsnma->crossAuth==TRUE_FLAG) && (wordType==2) && ((chOsnma->chDsm[0]!=0x00)))
		{
			chOsnma->crossAuth = FALSE_FLAG;
			RestartChannelOsnmaVariables(rxOsnma,chOsnma);
		}
		break;
	case CH_OSNMA_WAIT_VALID_NMA_HEADER:
		chOsnma->chDsmIndex++;
		chOsnma->chMackIndex += MACK_PER_WORD_SIZE;
		/* move from this state only when NMA header is different from 0x00 */
		if((wordType==2) && (chOsnma->chDsm[0]!=0x00))
		{
			/* OSNMA bits are back */
			chOsnma->crossAuth = FALSE_FLAG;
			if(wordOffset==chOsnma->chMackOffset)
			{
				/* MACK section starts here! */
				chOsnma->chMackIndex = MACK_PER_WORD_SIZE;
			}
			else
			{
				/* synch with MACK needed */
				chOsnma->chMackIndex = 0;
			}

			/* next state: synch still needed? */
			if((chOsnma->chDsmIndex==1) && (chOsnma->chMackIndex==MACK_PER_WORD_SIZE))
			{
				chOsnma->chOsnmaStatus = CH_OSNMA_OPERATIONAL;
			}
			else if(chOsnma->chDsmIndex==1)
			{
				chOsnma->chOsnmaStatus = CH_OSNMA_SYNC_MACK;
			}
			else if(chOsnma->chMackIndex==MACK_PER_WORD_SIZE)
			{
				chOsnma->chOsnmaStatus = CH_OSNMA_SYNC_DSM;
			}
		}
		break;
		/* other states: nothing to do
		 * this case should not be executed */
	default:
		return;
		break;
	}


	/* Save a DSM block when it is ready
	 * (DSM blocks are aligned with the navigation data subframes, so navigation message is saved also here) */
	if(chOsnma->chDsmIndex==GAL_PAGES_PER_SUBFRAME)
	{
		chOsnma->chDsmIndex = 0;

		/* the navigation message is saved only when some OSNMA valid bit is present */
		if(rxOsnma->osnmaTransmitted==TRUE_FLAG)
		{
			#ifdef OSNMA_PROFILING
			StartProfilingUpdate(&(rxOsnma->saveNavdataInMackList));
			#endif
			SaveNavdataInMackList(rxOsnma,chOsnma,wn,towSubframeStart);
			#ifdef OSNMA_PROFILING
			StopProfilingUpdate(&(rxOsnma->saveNavdataInMackList),1);
			#endif
		}

		/* if the current PRN is cross-authenticated, then OSNMA bits are not valid */
		if((rxOsnma->osnmaTransmitted==TRUE_FLAG) && (chOsnma->crossAuth==FALSE_FLAG))
		{
			/* current sat transmits OSNMA (self-authenticates), save DSM */
			#ifdef OSNMA_PROFILING
			StartProfilingUpdate(&(rxOsnma->saveDsmBlock));
			#endif
			SaveDsmBlock(rxOsnma,chOsnma,wn,towSubframeStart);
			#ifdef OSNMA_PROFILING
			StopProfilingUpdate(&(rxOsnma->saveDsmBlock),1);
			#endif

			/* Manage OSNMA parameters during renewals */
			if((dsmData->dsmType==DSM_KROOT)  && ((chOsnma->chDsm[1] & 0x0f)==0))
			{
				/* get the new OSNMA parameters, when BID==0 */
				if((chOsnma->rxOsnmaStatus==RX_OSNMA_NPK_WAIT_NOMINAL) ||
				   ((chOsnma->rxOsnmaStatus==RX_OSNMA_EOC_WAIT_NOMINAL) && (ExtractField(chOsnma->chDsm,3,3,2,0)!=pars->chainId) && (ExtractField(chOsnma->chDsm,3,3,2,0)!=parsNew->chainId)))
				{
					// if PK renewal, read them every time (needed to check when the new PK is in force)
					// if Chain renewal, read them only when they refer to the new chain but get them only once!
					GetOsnmaParameters(parsNew,chOsnma->chDsm,TRUE_FLAG,rxOsnma->parsLogFile);
				}

				/* manage the presence of floating root keys when a root key is not available yet
				 * and DSM-KROOT parameters must be replaced! */
				if((chOsnma->rxOsnmaStatus!=RX_OSNMA_NPK_WAIT_NOMINAL) && (chOsnma->rxOsnmaStatus!=RX_OSNMA_EOC_WAIT_NOMINAL) &&
				   (rxOsnma->KROOTValidityFLag==FALSE_FLAG) && (pars->parametersValid==TRUE_FLAG) && (pars->dsmId!=((chOsnma->chDsm[1]>>4) & 0x0f)))
				{
					GetOsnmaParameters(pars,chOsnma->chDsm,TRUE_FLAG,rxOsnma->parsLogFile);
					// the indexes of the TESLA keys read from MACK are wrong
					DeleteAllKeysAndMacsFromMackList(rxOsnma->mackList);
				}
			}
			
			/* choose the current parameters */
			if(dsmData->dsmType==DSM_KROOT)
			{
				if((chOsnma->rxOsnmaStatus==RX_OSNMA_EOC_WAIT_NOMINAL) && (dsmData->currDsmId==pars->dsmId))
				{
					parsTmp = pars;
				}
				else if((chOsnma->rxOsnmaStatus==RX_OSNMA_EOC_WAIT_NOMINAL) && (dsmData->currDsmId==parsNew->dsmId))
				{
					parsTmp = parsNew;
				}
				else if(chOsnma->rxOsnmaStatus==RX_OSNMA_NPK_WAIT_NOMINAL) 
				{
					parsTmp = parsNew;
				}
				else if(chOsnma->rxOsnmaStatus==RX_OSNMA_DU_WAIT_OP)
				{
					parsTmp = parsNew;
				}
				else
				{
					parsTmp = pars;
				}
			}
						
			/* Elaborate DSM-KROOT */
			if((dsmData->dsmType==DSM_KROOT) && (parsTmp->parametersValid==TRUE_FLAG))
			{	
				/* Verify signature, if dsmData is complete */
				// if((CheckDsmValidity(dsmData->validity,pars)==pars->dsmKrootNb) && (rxOsnma->KROOTValidityFLag==FALSE_FLAG))
				if(CheckDsmValidity(dsmData->validity,parsTmp->dsmKrootNb)==parsTmp->dsmKrootNb)
				{
					/* manage the change of the public key */
					if(((pars->parametersValid==TRUE_FLAG) && (parsNew->parametersValid==TRUE_FLAG) && (pars->pkId!=pars->dsmPkrNpkId) && (parsNew->pkId==pars->dsmPkrNpkId)) ||
					   ((pars->parametersValid==FALSE_FLAG) && (parsNew->parametersValid==TRUE_FLAG) && (parsNew->pkId==pars->dsmPkrNpkId)) ||
					   ((pars->parametersValid==TRUE_FLAG) && (pars->pkId==pars->dsmPkrNpkId) && (rxOsnma->evp_keyPkid!=pars->dsmPkrNpkId)))
					{
						// se la chiave pubblica NON è stata ricevuta, dsmPkrNpkId==-1 e quindi non entra nell'if
						if(rxOsnma->pkrValid==TRUE_FLAG)
						{
							if((rxOsnma->evp_keyPkid!=INVALID_VALUE) && (rxOsnma->evp_keyPkid!=pars->dsmPkrNpkId))
								SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The ECDSA Public Key is replaced (old PKID %d, new PKID %d)\n",rxOsnma->evp_keyPkid,parsNew->pkId);
							else if(pars->pkId!=parsNew->pkId)
								SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The ECDSA Public Key is replaced (old PKID %d, new PKID %d)\n",pars->pkId,parsNew->pkId);
							else
								SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The ECDSA Public Key is retrieved (new PKID %d)\n",parsNew->pkId);
							PublicKey_PkrToEVPKEY(pars->dsmPkrNpkt, rxOsnma->pkrPublicKey,(rxOsnma->pkrNpkLen/8),rxOsnma->evp_key);
							rxOsnma->evp_keyPkid = pars->dsmPkrNpkId;
							rxOsnma->evp_keyValid=TRUE_FLAG;
						}
						// copy parameters
						if(pars->parametersValid==TRUE_FLAG)
						{
							// if pars are already valid, do not overwrite the KROOT time
							CopyOsnmaParameters(parsNew,pars,FALSE_FLAG);
						}
						else
						{
							// if pars are not valid, overwrite the KROOT time
							CopyOsnmaParameters(parsNew,pars,TRUE_FLAG);
						}
						switch(pars->dsmPkrNpkt)
						{
							case ECDSA_P224:
								rxOsnma->function = EVP_sha224();
								pars->DSSizeBits = 448;
								break;
							case ECDSA_P256:
								rxOsnma->function = EVP_sha256();
								pars->DSSizeBits = 512;
								break;
							case ECDSA_P384:
								rxOsnma->function = EVP_sha384();
								pars->DSSizeBits = 768;
								break;
							case ECDSA_P521:
								rxOsnma->function = EVP_sha512();
								#ifdef PATCH_JRC_GENERATOR_DSM
								pars->DSSizeBits = 1042;
								#else
								pars->DSSizeBits = 1056;
								#endif
								break;
							default:
								break;
						}
						pars->DSSizeBytes=pars->DSSizeBits/8;
						#ifdef PATCH_JRC_GENERATOR_DSM
						if(pars->dsmPkrNpkt==ECDSA_P521)
						{
							pars->DSSizeBytes= 132;
						}
						#endif
						// update parameters that depend on the PK/DS length
						pars->M_DSSizeBits=pars->MSize*8+pars->DSSizeBits;
						pars->M_DSSizeBytes=ceil((double)pars->M_DSSizeBits/8);
						pars->LBits=(104*pars->dsmKrootNb)-pars->M_DSSizeBits;
						pars->LBytes=ceil((double)pars->LBits/8);
						//
						parsTmp->DSSizeBits=pars->DSSizeBits;
						parsTmp->DSSizeBytes = pars->DSSizeBytes;
						parsTmp->M_DSSizeBits = pars->M_DSSizeBits;
						parsTmp->M_DSSizeBytes = pars->M_DSSizeBytes;
						parsTmp->LBits = pars->LBits;
						parsTmp->LBytes = pars->LBytes;
					}
				
					#ifdef OSNMA_PROFILING
					StartProfilingUpdate(&(rxOsnma->dsmKrootProfiling));
					#endif
					DSVerifyDsmData=DSM_KROOT_SignatureVerification_fromEVPKEY(dsmData->dsmBlocks,rxOsnma,parsTmp,chOsnma);
					#ifdef OSNMA_PROFILING
					StopProfilingUpdate(&(rxOsnma->dsmKrootProfiling),1);
					StartProfilingUpdate(&(rxOsnma->P1Verification));
					#endif
					P1VerifyDsmData=DSM_KROOT_P1Verification(dsmData->dsmBlocks,rxOsnma,parsTmp);
					#ifdef OSNMA_PROFILING
					StopProfilingUpdate(&(rxOsnma->P1Verification),1);
					#endif
					dsmDataVerified=TRUE_FLAG;

					#ifdef OSNMA_DSM_KROOT_DEBUG
					{
						FILE *dbgFile = fopen("osnmaKRootValidation.txt","at");
						int dbgIdx1, dbgIdx2;
						unsigned char *dbgPtr = dsmData->dsmBlocks;

						fprintf(dbgFile,"%d      ",(int)DSVerifyDsmData);
						fprintf(dbgFile,"%6d ",towSubframeStart);
						fprintf(dbgFile,"%2d ",0);
						fprintf(dbgFile,"%2d ",dsmData->currDsmId);
						fprintf(dbgFile,"%2d     ",parsTmp->dsmKrootNb);
						for(dbgIdx1=0;dbgIdx1<parsTmp->dsmKrootNb;dbgIdx1++)
						{
							for(dbgIdx2=0;dbgIdx2<(DSM_BLOCK_SIZE_BITS/8);dbgIdx2++)
							{
								fprintf(dbgFile,"%3u ",(unsigned int)(*dbgPtr));
								dbgPtr++;
							}
						}
						fprintf(dbgFile,"\n");
						fclose(dbgFile);
					}
					#endif
				}
				/* Verify signature, if dsmArray[current channel] is complete */
				dsmElement = &(dsmData->dsmArray[chOsnma->chId]);
				if(CheckDsmValidity(dsmElement->validity,parsTmp->dsmKrootNb)==parsTmp->dsmKrootNb)
				{
					/* manage the change of the public key */
					if(((pars->parametersValid==TRUE_FLAG) && (parsNew->parametersValid==TRUE_FLAG) && (pars->pkId!=pars->dsmPkrNpkId) && (parsNew->pkId==pars->dsmPkrNpkId)) ||
					   ((pars->parametersValid==FALSE_FLAG) && (parsNew->parametersValid==TRUE_FLAG) && (parsNew->pkId==pars->dsmPkrNpkId)) ||
					   ((pars->parametersValid==TRUE_FLAG) && (pars->pkId==pars->dsmPkrNpkId) && (rxOsnma->evp_keyPkid!=pars->dsmPkrNpkId)))
					{
						// se la chiave pubblica NON è stata ricevuta, dsmPkrNpkId==-1 e quindi non entra nell'if
						if(rxOsnma->pkrValid==TRUE_FLAG)
						{
							if((rxOsnma->evp_keyPkid!=INVALID_VALUE) && (rxOsnma->evp_keyPkid!=pars->dsmPkrNpkId))
								SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The ECDSA Public Key is replaced (old PKID %d, new PKID %d)\n",rxOsnma->evp_keyPkid,parsNew->pkId);
							else if(pars->pkId!=parsNew->pkId)
								SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The ECDSA Public Key is replaced (old PKID %d, new PKID %d)\n",pars->pkId,parsNew->pkId);
							else
								SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The ECDSA Public Key is retrieved (new PKID %d)\n",parsNew->pkId);
							PublicKey_PkrToEVPKEY(pars->dsmPkrNpkt, rxOsnma->pkrPublicKey,(rxOsnma->pkrNpkLen/8),rxOsnma->evp_key);
							rxOsnma->evp_keyPkid = pars->dsmPkrNpkId;
							rxOsnma->evp_keyValid=TRUE_FLAG;
						}
						// copy parameters
						if(pars->parametersValid==TRUE_FLAG)
						{
							// if pars are already valid, do not overwrite the KROOT time
							CopyOsnmaParameters(parsNew,pars,FALSE_FLAG);
						}
						else
						{
							// if pars are not valid, overwrite the KROOT time
							CopyOsnmaParameters(parsNew,pars,TRUE_FLAG);
						}
						switch(pars->dsmPkrNpkt)
						{
							case ECDSA_P224:
								rxOsnma->function = EVP_sha224();
								pars->DSSizeBits = 448;
								break;
							case ECDSA_P256:
								rxOsnma->function = EVP_sha256();
								pars->DSSizeBits = 512;
								break;
							case ECDSA_P384:
								rxOsnma->function = EVP_sha384();
								pars->DSSizeBits = 768;
								break;
							case ECDSA_P521:
								rxOsnma->function = EVP_sha512();
								#ifdef PATCH_JRC_GENERATOR_DSM
								pars->DSSizeBits = 1042;
								#else
								pars->DSSizeBits = 1056;
								#endif
								break;
							default:
								break;
						}
						pars->DSSizeBytes = pars->DSSizeBits/8;
						#ifdef PATCH_JRC_GENERATOR_DSM
						if(pars->dsmPkrNpkt==ECDSA_P521)
						{
							pars->DSSizeBytes = 132;
						}
						#endif
						// update parameters that depend on the PK/DS length
						pars->M_DSSizeBits=pars->MSize*8+pars->DSSizeBits;
						pars->M_DSSizeBytes=ceil((double)pars->M_DSSizeBits/8);
						pars->LBits=(104*pars->dsmKrootNb)-pars->M_DSSizeBits;
						pars->LBytes=ceil((double)pars->LBits/8);
						//
						parsTmp->DSSizeBits=pars->DSSizeBits;
						parsTmp->DSSizeBytes = pars->DSSizeBytes;
						parsTmp->M_DSSizeBits = pars->M_DSSizeBits;
						parsTmp->M_DSSizeBytes = pars->M_DSSizeBytes;
						parsTmp->LBits = pars->LBits;
						parsTmp->LBytes = pars->LBytes;
					}
					
					#ifdef OSNMA_PROFILING
					StartProfilingUpdate(&(rxOsnma->dsmKrootProfiling));
					#endif
					DSVerifyDsmArray=DSM_KROOT_SignatureVerification_fromEVPKEY(dsmElement->dsmBlocks,rxOsnma,parsTmp,chOsnma);
					#ifdef OSNMA_PROFILING
					StopProfilingUpdate(&(rxOsnma->dsmKrootProfiling),1);
					StartProfilingUpdate(&(rxOsnma->P1Verification));
					#endif
					P1VerifyDsmArray=DSM_KROOT_P1Verification(dsmElement->dsmBlocks,rxOsnma,parsTmp);
					#ifdef OSNMA_PROFILING
					StopProfilingUpdate(&(rxOsnma->P1Verification),1);
					#endif
					dsmArrayDataVerified=TRUE_FLAG;
					/* log */
					if((DSVerifyDsmArray==SUCCESS_RETURN) && (P1VerifyDsmArray==SUCCESS_RETURN))
					{
						*(chOsnma->dsmStatusPtr) = VALIDATED;
					}
					else
					{
						*(chOsnma->dsmStatusPtr) = NOT_VALID;
					}

					#ifdef OSNMA_DSM_KROOT_DEBUG
					{
						FILE *dbgFile = fopen("osnmaKRootValidation.txt","at");
						int dbgIdx1, dbgIdx2;
						unsigned char *dbgPtr = dsmElement->dsmBlocks;

						fprintf(dbgFile,"%d      ",(int)DSVerifyDsmArray);
						fprintf(dbgFile,"%6d ",towSubframeStart);
						fprintf(dbgFile,"%2d ",chOsnma->prn);
						fprintf(dbgFile,"%2d ",dsmData->currDsmId);
						fprintf(dbgFile,"%2d     ",parsTmp->dsmKrootNb);
						for(dbgIdx1=0;dbgIdx1<parsTmp->dsmKrootNb;dbgIdx1++)
						{
							for(dbgIdx2=0;dbgIdx2<(DSM_BLOCK_SIZE_BITS/8);dbgIdx2++)
							{
								fprintf(dbgFile,"%3u ",(unsigned int)(*dbgPtr));
								dbgPtr++;
							}
						}
						fprintf(dbgFile,"\n");
						fclose(dbgFile);
					}
					#endif
				}
				#ifndef IGNORE_DSM_KROOT_SIGNATURE
				if((DSVerifyDsmArray==SUCCESS_RETURN) && (P1VerifyDsmArray==SUCCESS_RETURN) && (DSVerifyDsmData!=SUCCESS_RETURN) && (P1VerifyDsmData!=SUCCESS_RETURN))
				{
					memcpy(dsmData->dsmBlocks, dsmElement->dsmBlocks, parsTmp->dsmKrootNb*(DSM_BLOCK_SIZE_BITS/8));
					DSVerifyDsmData=SUCCESS_RETURN;
					P1VerifyDsmData=SUCCESS_RETURN;
				}
				if((DSVerifyDsmData==SUCCESS_RETURN) && (P1VerifyDsmData==SUCCESS_RETURN) && (rxOsnma->newKROOTValidityFLag==FALSE_FLAG) &&
				   ((chOsnma->rxOsnmaStatus==RX_OSNMA_EOC_WAIT_NOMINAL) && (parsTmp==parsNew) && (parsNew->chainId!=pars->chainId)))
				{
					// manage  the arrival of the new KROOT during a chain renewal
					memcpy(rxOsnma->newKROOT, &dsmData->dsmBlocks[13], parsNew->keySizeBytes);
					rxOsnma->newKROOTValidityFLag=TRUE_FLAG;
					SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The new Root Key has been received and validated (TESLA Chain Renewal)!\n");
				}
				else if((DSVerifyDsmData==SUCCESS_RETURN) && (P1VerifyDsmData==SUCCESS_RETURN) && (rxOsnma->KROOTValidityFLag==FALSE_FLAG) &&
						(chOsnma->rxOsnmaStatus==RX_OSNMA_DU_WAIT_OP))
				{
					memcpy(rxOsnma->KROOT, &dsmData->dsmBlocks[13], parsTmp->keySizeBytes);
					memcpy(rxOsnma->lastKeyVerified, &dsmData->dsmBlocks[13], parsTmp->keySizeBytes);

					#ifdef OSNMA_DSM_KROOT_DEBUG
					{
						FILE *rootLogFile = fopen("osnmaTeslaRoot.txt","a");
						int rootLogIndex;
						for(rootLogIndex=0;rootLogIndex<parsTmp->keySizeBytes;rootLogIndex++)
						{
							fprintf(rootLogFile,"%u ",rxOsnma->KROOT[rootLogIndex]);
						}
						fprintf(rootLogFile,"\n");
						fprintf(rootLogFile,"%d\n\n",parsTmp->krootTow);
						fclose(rootLogFile);
					}
					#endif

					rxOsnma->indexLastKeyVerified=0;
					rxOsnma->KROOTValidityFLag=TRUE_FLAG;
					SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The new Root Key has been received and validated (after a Revocation)!\n");
				}
				else if((DSVerifyDsmData==SUCCESS_RETURN) && (P1VerifyDsmData==SUCCESS_RETURN) && (parsTmp==parsNew) && (rxOsnma->KROOTValidityFLag==FALSE_FLAG) &&
						(chOsnma->rxOsnmaStatus==RX_OSNMA_NPK_WAIT_NOMINAL))
				{
					memcpy(rxOsnma->KROOT, &dsmData->dsmBlocks[13], parsNew->keySizeBytes);
					memcpy(rxOsnma->lastKeyVerified, &dsmData->dsmBlocks[13], parsNew->keySizeBytes);

					#ifdef OSNMA_DSM_KROOT_DEBUG
					{
						FILE *rootLogFile = fopen("osnmaTeslaRoot.txt","a");
						int rootLogIndex;
						for(rootLogIndex=0;rootLogIndex<parsNew->keySizeBytes;rootLogIndex++)
						{
							fprintf(rootLogFile,"%u ",rxOsnma->KROOT[rootLogIndex]);
						}
						fprintf(rootLogFile,"\n");
						fprintf(rootLogFile,"%d\n\n",parsNew->krootTow);
						fclose(rootLogFile);
					}
					#endif

					rxOsnma->indexLastKeyVerified=0;
					rxOsnma->KROOTValidityFLag=TRUE_FLAG;
					SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The Root Key has been received and validated (during a PK renewal)!\n");
				}
				else if((DSVerifyDsmData==SUCCESS_RETURN) && (P1VerifyDsmData==SUCCESS_RETURN) && (parsTmp==pars) && (rxOsnma->KROOTValidityFLag==FALSE_FLAG))
				{
					memcpy(rxOsnma->KROOT, &dsmData->dsmBlocks[13], pars->keySizeBytes);
					memcpy(rxOsnma->lastKeyVerified, &dsmData->dsmBlocks[13], pars->keySizeBytes);

					#ifdef OSNMA_DSM_KROOT_DEBUG
					{
						FILE *rootLogFile = fopen("osnmaTeslaRoot.txt","a");
						int rootLogIndex;
						for(rootLogIndex=0;rootLogIndex<pars->keySizeBytes;rootLogIndex++)
						{
							fprintf(rootLogFile,"%u ",rxOsnma->KROOT[rootLogIndex]);
						}
						fprintf(rootLogFile,"\n");
						fprintf(rootLogFile,"%d\n\n",pars->krootTow);
						fclose(rootLogFile);
					}
					#endif

					rxOsnma->indexLastKeyVerified=0;
					rxOsnma->KROOTValidityFLag=TRUE_FLAG;
					// memcpy (&pars->KROOT_WN, &dsmData->dsmBlocks[4], 2);
					// pars->KROOT_WN[0]= pars->KROOT_WN[0] & 0x0F;
					// pars->KROOT_WN[1]= pars->KROOT_WN[1] & 0xFF;
					// memcpy (&pars->KROOT_TOWH, &dsmData->dsmBlocks[6], 1);
					// memcpy (pars->alpha, &dsmData->dsmBlocks[7], 6);
					// memcpy (&pars->HF, &dsmData->dsmBlocks[1], 1);
					SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The Root Key has been received and validated!\n");
				}
				#else
				if((dsmArrayDataVerified==TRUE_FLAG) && (dsmDataVerified==FALSE_FLAG))
				{
					memcpy(dsmData->dsmBlocks, dsmElement->dsmBlocks, parsTmp->dsmKrootNb*(DSM_BLOCK_SIZE_BITS/8));
					dsmDataVerified=TRUE_FLAG;
				}
				if((dsmDataVerified==TRUE_FLAG) && (rxOsnma->newKROOTValidityFLag==FALSE_FLAG) &&
				   ((chOsnma->rxOsnmaStatus==RX_OSNMA_EOC_WAIT_NOMINAL) && (parsTmp==parsNew) && (parsNew->chainId!=pars->chainId)))
				{
					// manage  the arrival of the new KROOT dusing a chain renewal
					memcpy(rxOsnma->newKROOT, &dsmData->dsmBlocks[13], parsNew->keySizeBytes);
					rxOsnma->newKROOTValidityFLag=TRUE_FLAG;

					if((DSVerifyDsmData==SUCCESS_RETURN) && (P1VerifyDsmData==SUCCESS_RETURN))
					{
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The new Root Key has been received and validated (TESLA Chain Renewal)!\n");
					}
					else if((DSVerifyDsmArray==SUCCESS_RETURN) && (P1VerifyDsmArray==SUCCESS_RETURN))
					{
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The new Root Key has been received and validated (from PRN %d) (TESLA Chain Renewal)!\n",chOsnma->prn);
					}
					else
					{
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The received new Root Key is NOT validated. Nevertheless use it (TESLA Chain Renewal).\n");
					}
				}
				else if((dsmDataVerified==TRUE_FLAG) && (rxOsnma->KROOTValidityFLag==FALSE_FLAG) &&
						(chOsnma->rxOsnmaStatus==RX_OSNMA_DU_WAIT_OP))
				{
					memcpy(rxOsnma->KROOT, &dsmData->dsmBlocks[13], pars->keySizeBytes);
					memcpy(rxOsnma->lastKeyVerified, &dsmData->dsmBlocks[13], pars->keySizeBytes);
					rxOsnma->indexLastKeyVerified=0;
					rxOsnma->KROOTValidityFLag=TRUE_FLAG;
					if((DSVerifyDsmData==SUCCESS_RETURN) && (P1VerifyDsmData==SUCCESS_RETURN))
					{
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The new Root Key has been received and validated (after a Revocation)!\n");
					}
					else if((DSVerifyDsmArray==SUCCESS_RETURN) && (P1VerifyDsmArray==SUCCESS_RETURN))
					{
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The new Root Key has been received and validated (from PRN %d) ((after a Revocation)!\n",chOsnma->prn);
					}
					else
					{
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The received new Root Key is NOT validated. Nevertheless use it ((after a Revocation).\n");
					}
				}
				else if((dsmDataVerified==TRUE_FLAG) && (parsTmp==pars) && (rxOsnma->KROOTValidityFLag==FALSE_FLAG))
				{
					memcpy(rxOsnma->KROOT, &dsmData->dsmBlocks[13], pars->keySizeBytes);
					memcpy(rxOsnma->lastKeyVerified, &dsmData->dsmBlocks[13], pars->keySizeBytes);
					rxOsnma->indexLastKeyVerified=0;
					rxOsnma->KROOTValidityFLag=TRUE_FLAG;

					if((DSVerifyDsmData==SUCCESS_RETURN) && (P1VerifyDsmData==SUCCESS_RETURN))
					{
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The Root Key has been received and validated!\n");
					}
					else if((DSVerifyDsmArray==SUCCESS_RETURN) && (P1VerifyDsmArray==SUCCESS_RETURN))
					{
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The Root Key has been received and validated (from PRN %d)!\n",chOsnma->prn);
					}
					else
					{
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The received Root Key is NOT validated. Nevertheless use it.\n");
					}
				}
				#endif

				/* validation log (DSM-KROOT) */
				if(dsmDataVerified==TRUE_FLAG)
				{
					if((DSVerifyDsmData==SUCCESS_RETURN) && (P1VerifyDsmData==SUCCESS_RETURN) && (rxOsnma->validationLogFile!=NULL))
					{
						fprintf(rxOsnma->validationLogFile,"%2d  %4d  %6d  %4d  %6d  %3d  %2d  %2d  %4d  %6d  %9d  %2d  %3d  %2d  %d\n",LOG_FLAG_ROOT_KEY_VALIDATION,wn,tow+2,-1,-1,OSNMA_GENERAL_GALILEO_INFO,-1,pars->chainId,pars->krootWn,pars->krootTow,-1,-1,-1,-1,1);
					}
					else if((DSVerifyDsmData==FAILURE_RETURN) && (P1VerifyDsmData==FAILURE_RETURN) && (rxOsnma->validationLogFile!=NULL))
					{
						fprintf(rxOsnma->validationLogFile,"%2d  %4d  %6d  %4d  %6d  %3d  %2d  %2d  %4d  %6d  %9d  %2d  %3d  %2d  %d\n",LOG_FLAG_ROOT_KEY_VALIDATION,wn,tow+2,-1,-1,OSNMA_GENERAL_GALILEO_INFO,-1,pars->chainId,pars->krootWn,pars->krootTow,-1,-1,-1,-1,0);
					}
					// if the verification has been skipped (missing PK), write nothing
					ClearDsmDataValidity(dsmData);
				}
				if(dsmArrayDataVerified==TRUE_FLAG)
				{
					if((DSVerifyDsmArray==SUCCESS_RETURN) && (P1VerifyDsmArray==SUCCESS_RETURN) && (rxOsnma->validationLogFile!=NULL))
					{
						fprintf(rxOsnma->validationLogFile,"%2d  %4d  %6d  %4d  %6d  %3d  %2d  %2d  %4d  %6d  %9d  %2d  %3d  %2d  %d\n",LOG_FLAG_ROOT_KEY_VALIDATION,wn,tow+2,-1,-1,chOsnma->prn,-1,pars->chainId,pars->krootWn,pars->krootTow,-1,-1,-1,-1,1);
					}
					else if((DSVerifyDsmArray==FAILURE_RETURN) && (P1VerifyDsmArray==FAILURE_RETURN) && (rxOsnma->validationLogFile!=NULL))
					{
						fprintf(rxOsnma->validationLogFile,"%2d  %4d  %6d  %4d  %6d  %3d  %2d  %2d  %4d  %6d  %9d  %2d  %3d  %2d  %d\n",LOG_FLAG_ROOT_KEY_VALIDATION,wn,tow+2,-1,-1,chOsnma->prn,-1,pars->chainId,pars->krootWn,pars->krootTow,-1,-1,-1,-1,0);
					}
					// if the verification has been skipped (missing PK), write nothing
					ClearDsmValidityOneChannel(dsmData,chOsnma->chId);
				}
			} // End elaboration DSM-KROOT

			/* Elaborate DSM-PKR */
			if((dsmData->dsmType==DSM_PKR) && (pars->dsmPkrNb>0)) // if dsmPkrNb<0, then DSM-PKR parameters are not valid
			{
				//if((rxOsnma->evp_keyValid==FALSE_FLAG) && (CheckDsmValidity(dsmData->validity,pars->dsmPkrNb)==pars->dsmPkrNb))
				if(CheckDsmValidity(dsmData->validity,pars->dsmPkrNb)==pars->dsmPkrNb)
				{
					/* Get the Padding (P2) length */
					#ifdef PATCH_JRC_GENERATOR_ALERT
					// 1040 = size bits of Nb of blocks + Message ID + Intermediate Nodes + NPKT + NPKID
					rxOsnma->pkrP2Len = (pars->dsmPkrNb*DSM_BLOCK_SIZE_BITS) - 1040 - rxOsnma->pkrNpkLen;
					#else
					if(pars->dsmPkrNpkt!=EMERGENCY_SERVICE_MESSAGE)
					{
						// 1040 = size bits of Nb of blocks + Message ID + Intermediate Nodes + NPKT + NPKID
						rxOsnma->pkrP2Len = (pars->dsmPkrNb*DSM_BLOCK_SIZE_BITS) - 1040 - rxOsnma->pkrNpkLen;
					}
					else
					{
						// when Emergency (Alert) Message, the "key" uses all the available bits, no padding
						// 1040 = size bits of Nb of blocks + Message ID + Intermediate Nodes + NPKT + NPKID
						rxOsnma->pkrNpkLen = (pars->dsmPkrNb*DSM_BLOCK_SIZE_BITS) - 1040;
						rxOsnma->pkrP2Len = 0;
					}
					#endif
					/* Parse */
					memcpy(rxOsnma->mtIntermediateNodes,&(dsmData->dsmBlocks[1]),4*(MERKLE_TREE_NODE_SIZE/8)); // skip 8 bits

					memcpy(rxOsnma->pkrPublicKeyTmp,&(dsmData->dsmBlocks[130]),rxOsnma->pkrNpkLen/8); // skip 1040 bits
					memcpy(rxOsnma->pkrP2,&(dsmData->dsmBlocks[130+(rxOsnma->pkrNpkLen/8)]),rxOsnma->pkrP2Len/8); // skip 1040 bits + the key
					//*************************************************
					#ifdef OSNMA_PROFILING
					StartProfilingUpdate(&(rxOsnma->merkleTreeVerification));
					#endif
					PKVerifyDsmData=MerkleTreePublicKeyVerification(rxOsnma,pars,dsmData->dsmBlocks);
					#ifdef OSNMA_PROFILING
					StopProfilingUpdate(&(rxOsnma->merkleTreeVerification),1);
					StartProfilingUpdate(&(rxOsnma->P2Verification));
					#endif
					P2VerifyDsmData=DSM_PKR_P2Verification(rxOsnma,pars);
					#ifdef OSNMA_PROFILING
					StopProfilingUpdate(&(rxOsnma->P2Verification),1);
					#endif
					#ifndef IGNORE_MERKLE_TREE
					if(PKVerifyDsmData==SUCCESS_RETURN && P2VerifyDsmData==SUCCESS_RETURN && pars->dsmPkrNpkt==EMERGENCY_SERVICE_MESSAGE)
					{
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"Received a valid OSNMA Alert Message from OSNMA!\n");
						rxOsnma->pkrOam = TRUE_FLAG;
					}
					else if(pars->dsmPkrNpkt==EMERGENCY_SERVICE_MESSAGE)
					{
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The received OSNMA Alert Message is NOT validated.");
					}
					else if(PKVerifyDsmData==SUCCESS_RETURN && P2VerifyDsmData==SUCCESS_RETURN && rxOsnma->pkrValid==FALSE_FLAG)
					{
						memcpy(rxOsnma->pkrPublicKey,rxOsnma->pkrPublicKeyTmp,rxOsnma->pkrNpkLen/8);
						rxOsnma->pkrValid = TRUE_FLAG;
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"Received a valid ECDSA Compressed Public Key from OSNMA!\n");
						if(rxOsnma->evp_keyValid==FALSE_FLAG)
						{
							rxOsnma->evp_keyPkid = pars->dsmPkrNpkId;
							rxOsnma->evp_keyValid=TRUE_FLAG;
							PublicKey_PkrToEVPKEY(pars->dsmPkrNpkt, rxOsnma->pkrPublicKey,(rxOsnma->pkrNpkLen/8),rxOsnma->evp_key);
						}
					}
					else if(rxOsnma->pkrValid==FALSE_FLAG)
					{
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The received ECDSA Compressed Public Key is NOT validated.");
					}
					#else
					if(PKVerifyDsmData==SUCCESS_RETURN && P2VerifyDsmData==SUCCESS_RETURN && pars->dsmPkrNpkt==EMERGENCY_SERVICE_MESSAGE)
					{
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"Received a valid OSNMA Alert Message from OSNMA!\n");
						rxOsnma->pkrOam = TRUE_FLAG;
					}
					else if(pars->dsmPkrNpkt==EMERGENCY_SERVICE_MESSAGE)
					{
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The received OSNMA Alert Message is NOT validated. Nevertheless use it.\n");
						rxOsnma->pkrOam = TRUE_FLAG;
					}
					else if(PKVerifyDsmData==SUCCESS_RETURN && P2VerifyDsmData==SUCCESS_RETURN && rxOsnma->pkrValid==FALSE_FLAG)
					{
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"Received a valid ECDSA Compressed Public Key from OSNMA!\n");
					}
					else if(rxOsnma->pkrValid==FALSE_FLAG)
					{
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The received ECDSA Compressed Public Key is NOT validated. Nevertheless use it.\n");
					}
					memcpy(rxOsnma->pkrPublicKey,rxOsnma->pkrPublicKeyTmp,rxOsnma->pkrNpkLen/8);
					rxOsnma->pkrValid = TRUE_FLAG;
					if(rxOsnma->evp_keyValid==FALSE_FLAG)
					{
						rxOsnma->evp_keyValid=TRUE_FLAG;
						PublicKey_PkrToEVPKEY(pars->dsmPkrNpkt, rxOsnma->pkrPublicKey,(rxOsnma->pkrNpkLen/8),rxOsnma->evp_key);
					}
					#endif

					/* validation log (DSM-PKR) */
					if((PKVerifyDsmData==SUCCESS_RETURN) && (P2VerifyDsmData==SUCCESS_RETURN) && (rxOsnma->validationLogFile!=NULL))
					{
						fprintf(rxOsnma->validationLogFile,"%2d  %4d  %6d  %4d  %6d  %3d  %2d  %2d  %4d  %6d  %9d  %2d  %3d  %2d  %d\n",LOG_FLAG_PK_VALIDATION,wn,tow+2,-1,-1,OSNMA_GENERAL_GALILEO_INFO,pars->dsmPkrNpkId,-1,-1,-1,-1,-1,-1,-1,1);
					}
					else if(rxOsnma->validationLogFile!=NULL)
					{
						fprintf(rxOsnma->validationLogFile,"%2d  %4d  %6d  %4d  %6d  %3d  %2d  %2d  %4d  %6d  %9d  %2d  %3d  %2d  %d\n",LOG_FLAG_PK_VALIDATION,wn,tow+2,-1,-1,OSNMA_GENERAL_GALILEO_INFO,pars->dsmPkrNpkId,-1,-1,-1,-1,-1,-1,-1,0);
					}
					
					ClearDsmDataValidity(dsmData);
				}
				dsmElement = &(dsmData->dsmArray[chOsnma->chId]);
				//if((rxOsnma->evp_keyValid==FALSE_FLAG) && (CheckDsmValidity(dsmElement->validity,pars->dsmPkrNb)==pars->dsmPkrNb))
				if(CheckDsmValidity(dsmElement->validity,pars->dsmPkrNb)==pars->dsmPkrNb)
				{
					/* Get the Padding (P2) length */
					#ifdef PATCH_JRC_GENERATOR_ALERT
					// 1040 = size bits of Nb of blocks + Message ID + Intermediate Nodes + NPKT + NPKID
					rxOsnma->pkrP2Len = (pars->dsmPkrNb*DSM_BLOCK_SIZE_BITS) - 1040 - rxOsnma->pkrNpkLen;
					#else
					if(pars->dsmPkrNpkt!=EMERGENCY_SERVICE_MESSAGE)
					{
						// 1040 = size bits of Nb of blocks + Message ID + Intermediate Nodes + NPKT + NPKID
						rxOsnma->pkrP2Len = (pars->dsmPkrNb*DSM_BLOCK_SIZE_BITS) - 1040 - rxOsnma->pkrNpkLen;
					}
					else
					{
						// when Emergency (Alert) Message, the "key" uses all the available bits, no padding
						// 1040 = size bits of Nb of blocks + Message ID + Intermediate Nodes + NPKT + NPKID
						rxOsnma->pkrNpkLen = (pars->dsmPkrNb*DSM_BLOCK_SIZE_BITS) - 1040;
						rxOsnma->pkrP2Len = 0;
					}
					#endif
					/* Parse */
					memcpy(rxOsnma->mtIntermediateNodes,&(dsmElement->dsmBlocks[1]),4*(MERKLE_TREE_NODE_SIZE/8)); // skip 8 bits
					memcpy(rxOsnma->pkrPublicKeyTmp,&(dsmElement->dsmBlocks[130]),rxOsnma->pkrNpkLen/8); // skip 1040 bits
					memcpy(rxOsnma->pkrP2,&(dsmElement->dsmBlocks[130+(rxOsnma->pkrNpkLen/8)]),rxOsnma->pkrP2Len/8); // skip 1040 bits + the key
					#ifdef OSNMA_PROFILING
					StartProfilingUpdate(&(rxOsnma->merkleTreeVerification));
					#endif
					PKVerifyDsmArray=MerkleTreePublicKeyVerification(rxOsnma,pars,dsmElement->dsmBlocks);
					#ifdef OSNMA_PROFILING
					StopProfilingUpdate(&(rxOsnma->merkleTreeVerification),1);
					StartProfilingUpdate(&(rxOsnma->P2Verification));
					#endif
					P2VerifyDsmArray=DSM_PKR_P2Verification(rxOsnma,pars);
					#ifdef OSNMA_PROFILING
					StopProfilingUpdate(&(rxOsnma->P2Verification),1);
					#endif
					/* log */
					if((PKVerifyDsmArray==SUCCESS_RETURN) && (P2VerifyDsmArray==SUCCESS_RETURN))
					{
						*(chOsnma->dsmStatusPtr) = VALIDATED;
					}
					else
					{
						*(chOsnma->dsmStatusPtr) = NOT_VALID;
					}
					#ifndef IGNORE_MERKLE_TREE
					if(PKVerifyDsmArray==SUCCESS_RETURN && P2VerifyDsmArray==SUCCESS_RETURN && pars->dsmPkrNpkt==EMERGENCY_SERVICE_MESSAGE)
					{
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"Received a valid OSNMA Alert Message from OSNMA!\n");
						rxOsnma->pkrOam = TRUE_FLAG;
					}
					else if(pars->dsmPkrNpkt==EMERGENCY_SERVICE_MESSAGE)
					{
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The received OSNMA Alert Message is NOT validated.");
					}
					else if(PKVerifyDsmArray==SUCCESS_RETURN && P2VerifyDsmArray==SUCCESS_RETURN && rxOsnma->pkrValid==FALSE_FLAG)
					{
						memcpy(rxOsnma->pkrPublicKey,rxOsnma->pkrPublicKeyTmp,rxOsnma->pkrNpkLen/8);
						rxOsnma->pkrValid = TRUE_FLAG;
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"Received a valid ECDSA Compressed Public Key from OSNMA!\n");
						if(rxOsnma->evp_keyValid==FALSE_FLAG)
						{
							rxOsnma->evp_keyPkid = pars->dsmPkrNpkId;
							rxOsnma->evp_keyValid=TRUE_FLAG;
							PublicKey_PkrToEVPKEY(pars->dsmPkrNpkt, rxOsnma->pkrPublicKey,(rxOsnma->pkrNpkLen/8),rxOsnma->evp_key);
						}
					}
					else if(rxOsnma->pkrValid==FALSE_FLAG)
					{
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The received ECDSA Compressed Public Key is NOT validated.");
					}
					#else
					if(PKVerifyDsmArray==SUCCESS_RETURN && P2VerifyDsmArray==SUCCESS_RETURN && pars->dsmPkrNpkt==EMERGENCY_SERVICE_MESSAGE)
					{
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"Received a valid OSNMA Alert Message from OSNMA!\n");
						rxOsnma->pkrOam = TRUE_FLAG;
					}
					else if(pars->dsmPkrNpkt==EMERGENCY_SERVICE_MESSAGE)
					{
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The received OSNMA Alert Message is NOT validated. Nevertheless use it.\n");
						rxOsnma->pkrOam = TRUE_FLAG;
					}
					else if(PKVerifyDsmArray==SUCCESS_RETURN && P2VerifyDsmArray==SUCCESS_RETURN && rxOsnma->pkrValid==FALSE_FLAG)
					{
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"Received a valid ECDSA Compressed Public Key from OSNMA!\n");
					}
					else if(rxOsnma->pkrValid==FALSE_FLAG)
					{
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The received ECDSA Compressed Public Key is NOT validated. Nevertheless use it.\n");
					}
					memcpy(rxOsnma->pkrPublicKey,rxOsnma->pkrPublicKeyTmp,rxOsnma->pkrNpkLen/8);
					rxOsnma->pkrValid = TRUE_FLAG;
					if(rxOsnma->evp_keyValid==FALSE_FLAG)
					{
						rxOsnma->evp_keyValid=TRUE_FLAG;
						PublicKey_PkrToEVPKEY(pars->dsmPkrNpkt, rxOsnma->pkrPublicKey,(rxOsnma->pkrNpkLen/8),rxOsnma->evp_key);
					}
					#endif

					/* validation log (DSM-PKR) */
					if((PKVerifyDsmArray==SUCCESS_RETURN) && (P2VerifyDsmArray==SUCCESS_RETURN) && (rxOsnma->validationLogFile!=NULL))
					{
						fprintf(rxOsnma->validationLogFile,"%2d  %4d  %6d  %4d  %6d  %3d  %2d  %2d  %4d  %6d  %9d  %2d  %3d  %2d  %d\n",LOG_FLAG_PK_VALIDATION,wn,tow+2,-1,-1,chOsnma->prn,pars->dsmPkrNpkId,-1,-1,-1,-1,-1,-1,-1,1);
					}
					else if(rxOsnma->validationLogFile!=NULL)
					{
						fprintf(rxOsnma->validationLogFile,"%2d  %4d  %6d  %4d  %6d  %3d  %2d  %2d  %4d  %6d  %9d  %2d  %3d  %2d  %d\n",LOG_FLAG_PK_VALIDATION,wn,tow+2,-1,-1,chOsnma->prn,pars->dsmPkrNpkId,-1,-1,-1,-1,-1,-1,-1,0);
					}
					ClearDsmValidityOneChannel(dsmData,chOsnma->chId);
				}
			} // if(dsmData->dsmType==DSM_PKR)
		} // if((rxOsnma->osnmaTransmitted==TRUE_FLAG) && (chOsnma->crossAuth==FALSE_FLAG))
	} // if(chOsnma->chDsmIndex==GAL_PAGES_PER_SUBFRAME)

	/* Save a MACK Section when it is ready */
	if(chOsnma->chMackIndex==MACK_PER_SUBFRAME_SIZE)
	{
		chOsnma->chMackIndex = 0;
		if((pars->parametersValid==TRUE_FLAG) && (chOsnma->crossAuth==FALSE_FLAG))
		{
			/* current sat transmits OSNMA (self-authenticates), save MACK */
			#ifdef OSNMA_PROFILING
			StartProfilingUpdate(&(rxOsnma->saveMackInMackList));
			#endif
			SaveMackInMackList(rxOsnma,chOsnma,wn,towSubframeStart);
			#ifdef OSNMA_PROFILING
			StopProfilingUpdate(&(rxOsnma->saveMackInMackList),1);
			#endif
		}
	}

	/* prepare the log string */
	sprintf(chOsnma->chOsnmaStatusLog,"0x%02x%02x%02x%02x%02x   ",rsvd1[0],rsvd1[1],rsvd1[2],rsvd1[3],rsvd1[4]);
	charPtr = &(chOsnma->chOsnmaStatusLog[15]);
	if(rxOsnma->osnmaTransmitted==FALSE_FLAG)
	{
		sprintf(charPtr,"OSNMA absent");
		chOsnma->chOsnmaStatusLogColor = NC_COLOR_SCHEME_NO_OSNMA;
		*(chOsnma->dsmStatusPtr) = TO_BE_VALIDATED;
		*(chOsnma->keyStatusPtr) = TO_BE_VALIDATED;
		*(chOsnma->msgStatusPtr) = TO_BE_VALIDATED;
		*(chOsnma->macseqStatusPtr) = TO_BE_VALIDATED;
	}
	else if((chOsnma->rxOsnmaStatus==RX_OSNMA_DU_WAIT_OP) && (chOsnma->cpks==CREV))
	{
		sprintf(charPtr,"Chain Revocation");
		chOsnma->chOsnmaStatusLogColor = NC_COLOR_SCHEME_OSNMA_BAD;
		*(chOsnma->keyStatusPtr) = TO_BE_VALIDATED;
		*(chOsnma->msgStatusPtr) = TO_BE_VALIDATED;
		*(chOsnma->macseqStatusPtr) = TO_BE_VALIDATED;
	}
	else if((chOsnma->rxOsnmaStatus==RX_OSNMA_DU_WAIT_OP) && (chOsnma->cpks==PKREV))
	{
		sprintf(charPtr,"PK Revocation");
		chOsnma->chOsnmaStatusLogColor = NC_COLOR_SCHEME_OSNMA_BAD;
		*(chOsnma->keyStatusPtr) = TO_BE_VALIDATED;
		*(chOsnma->msgStatusPtr) = TO_BE_VALIDATED;
		*(chOsnma->macseqStatusPtr) = TO_BE_VALIDATED;
	}
	else if(chOsnma->rxOsnmaStatus==RX_OSNMA_EMERGENCY)
	{
		sprintf(charPtr,"Alert Message");
		chOsnma->chOsnmaStatusLogColor = NC_COLOR_SCHEME_OSNMA_BAD;
		*(chOsnma->keyStatusPtr) = TO_BE_VALIDATED;
		*(chOsnma->msgStatusPtr) = TO_BE_VALIDATED;
		*(chOsnma->macseqStatusPtr) = TO_BE_VALIDATED;
	}
	else if(chOsnma->chOsnmaStatus==CH_OSNMA_IDLE)
	{
		sprintf(charPtr,"OSNMA present");
		chOsnma->chOsnmaStatusLogColor = NC_COLOR_SCHEME_NO_OSNMA;
	}
	else if(chOsnma->chOsnmaStatus==CH_OSNMA_WAIT_VALID_NMA_HEADER)
	{
		sprintf(charPtr,"OSNMA Cross Auth.");
		chOsnma->chOsnmaStatusLogColor = NC_COLOR_SCHEME_NO_OSNMA;
		*(chOsnma->dsmStatusPtr) = TO_BE_VALIDATED;
		*(chOsnma->keyStatusPtr) = TO_BE_VALIDATED;
		*(chOsnma->macseqStatusPtr) = TO_BE_VALIDATED;
		if(rxOsnma->revocationStarted==TRUE_FLAG)
			*(chOsnma->msgStatusPtr) = TO_BE_VALIDATED;
	}
	else if(pars->parametersValid==FALSE_FLAG)
	{
		sprintf(charPtr,"OSNMA present");
		chOsnma->chOsnmaStatusLogColor = NC_COLOR_SCHEME_OSNMA_OP;
	}
	else if(chOsnma->chOsnmaStatus==CH_OSNMA_OPERATIONAL)
	{
		nmaStatus = (TNmaStatus)(chOsnma->chDsm[0]>>6);
		if(chOsnma->crossAuth==TRUE_FLAG)
		{
			sprintf(charPtr,"OSNMA Cross Auth.");
			chOsnma->chOsnmaStatusLogColor = NC_COLOR_SCHEME_OSNMA_OP;
			*(chOsnma->dsmStatusPtr) = TO_BE_VALIDATED;
			*(chOsnma->keyStatusPtr) = TO_BE_VALIDATED;
			*(chOsnma->macseqStatusPtr) = TO_BE_VALIDATED;
			if(rxOsnma->revocationStarted==TRUE_FLAG)
				*(chOsnma->msgStatusPtr) = TO_BE_VALIDATED;
		}
		else if(nmaStatus==NMA_DO_NOT_USE)
		{
			sprintf(charPtr,"OSNMA Don't Use");
			chOsnma->chOsnmaStatusLogColor = NC_COLOR_SCHEME_OSNMA_BAD;
			*(chOsnma->keyStatusPtr) = TO_BE_VALIDATED;
			*(chOsnma->msgStatusPtr) = TO_BE_VALIDATED;
			*(chOsnma->macseqStatusPtr) = TO_BE_VALIDATED;
		}
		else if(nmaStatus==NMA_TEST)
		{
			sprintf(charPtr,"OSNMA Test");
			chOsnma->chOsnmaStatusLogColor = NC_COLOR_SCHEME_OSNMA_TEST;
		}
		else if(nmaStatus==NMA_OPERATIONAL)
		{
			sprintf(charPtr,"OSNMA Operational");
			chOsnma->chOsnmaStatusLogColor = NC_COLOR_SCHEME_OSNMA_OP;
		}
	}
	else
	{
		/* the nmaStatus is not valid */
		sprintf(charPtr,"OSNMA present");
		chOsnma->chOsnmaStatusLogColor = NC_COLOR_SCHEME_OSNMA_OP;
	}
}


void RestartChannelOsnmaVariables(TOsnmaData *rxOsnma, TChannelOsnmaVariables *chOsnma)
{
	if(chOsnma->chOsnmaStatus==CH_OSNMA_IDLE)
	{
		/* NMA Header different from 0 not found (yet) */
		return;
	}

	/* used when some word is lost, e.g. due to CRC errors */
	chOsnma->chDsmIndex = 0;
	chOsnma->chMackIndex = 0;
	/* synchronization must be repeated */
	if(rxOsnma->osnmaPars.parametersValid==TRUE_FLAG)
	{
		chOsnma->chOsnmaStatus = CH_OSNMA_SYNC_MACK_DSM;
	}
	else
	{
		/* if OSNMA parameters are not available, start from the initial transient! */
		chOsnma->chOsnmaStatus = CH_OSNMA_WAIT_SUBFRAME_START;
	}
	/* logs */
	*(chOsnma->dsmStatusPtr) = TO_BE_VALIDATED;
	*(chOsnma->keyStatusPtr) = TO_BE_VALIDATED;
	*(chOsnma->msgStatusPtr) = TO_BE_VALIDATED;
	*(chOsnma->macseqStatusPtr) = TO_BE_VALIDATED;
	sprintf(chOsnma->chOsnmaStatusLog," ");
}


/**
 *
 */
static void UpdateByteIndexBitIndex(int *startByte,int *endByte,int *bitsOfFirstByte,int *bitsOfLastByte,int numBits)
{
	int byteIdx1;
	int bitIdx1;
	int byteIdx2;
	int bitIdx2;

	/* for sake of simplicity, use the byte and bit index (being 0 the index of the MS bit in each byte, and 7 the last one) */
	/* init */
	if(*bitsOfLastByte==8)
	{
		/* pass to the next byte */
		byteIdx1 = (*endByte) +1;
		bitIdx1 = 0;
	}
	else
	{
		byteIdx1 = (*endByte);
		bitIdx1 = (*bitsOfLastByte);
	}
	/* update */
	byteIdx2 = byteIdx1 + (numBits/8);
	bitIdx2 = bitIdx1 + (numBits%8) -1;
	if(bitIdx2>=8)
	{
		/* pass to the next byte */
		byteIdx2++;
		bitIdx2 -= 8;
	}
	else if(bitIdx2<0)
	{
		/* the bit is the last of the previous byte */
		byteIdx2--;
		bitIdx2 = 7;
	}
	/* translate into format required by parsing functions! */
	*startByte = byteIdx1;
	*endByte = byteIdx2;
	*bitsOfFirstByte = 8-bitIdx1;
	*bitsOfLastByte = bitIdx2 +1;
}


int CheckTeslaValidationThread(TOsnmaData *rxOsnma)
{
	TOsnmaParameters *pars = &(rxOsnma->osnmaPars);
	int threadTESLAKeyVerificationIsRunningFlagToRead;
	int index;
	int prnA = teslaKeyVerificationThFuncParam.prnA;
	int indexKey = teslaKeyVerificationThFuncParam.indexKeyToStart;
	int wnMackStart = teslaKeyVerificationThFuncParam.wnMackStart;
	int towMackStart = teslaKeyVerificationThFuncParam.towMackStart;

	if(rxOsnma->indexLastKeyVerified!=-1)
	{
		pthread_mutex_lock(&(rxOsnma->TESLAKeyVerificationMutex));
		threadTESLAKeyVerificationIsRunningFlagToRead=threadTESLAKeyVerificationStopIsRunning;
		pthread_mutex_unlock(&(rxOsnma->TESLAKeyVerificationMutex));

		/*If the thread is not running or has finished to run*/
		if(threadTESLAKeyVerificationIsRunningFlagToRead==FALSE_FLAG)
		{
			/* If the thread has verified a key, then copy the result from the thread structure into the corresponding receiver structure */
			if(rxOsnma->indexLastKeyVerifiedThread!=-1 && rxOsnma->indexLastKeyVerifiedThread!=rxOsnma->indexLastKeyVerified)
			{
				rxOsnma->indexLastKeyVerified=rxOsnma->indexLastKeyVerifiedThread;
				memcpy(rxOsnma->lastKeyVerified,rxOsnma->lastKeyVerifiedThread,pars->keySize/BITS_PER_BYTE);
				rxOsnma->indexLastKeyVerifiedThread=-1;
				/**/
				for(index=0;index<MAX_NMACK_BLOCK;index++)
				{
					memcpy(rxOsnma->lastPrn1Key[index],rxOsnma->lastPrn1KeyThread[index],pars->keySize/BITS_PER_BYTE);
					memcpy(rxOsnma->lastPrn19Key[index],rxOsnma->lastPrn19KeyThread[index],pars->keySize/BITS_PER_BYTE);
					rxOsnma->indexLastPrn1Key[index]=rxOsnma->indexLastPrn1KeyThread[index];
					rxOsnma->wnLastPrn1Key[index]=rxOsnma->wnLastPrn1KeyThread[index];/*GST WN of the last PRN1 key saved*/
					rxOsnma->towLastPrn1Key[index]=rxOsnma->towLastPrn1KeyThread[index];/*GST TOW of the last PRN1 key saved*/
					rxOsnma->indexLastPrn19Key[index]=rxOsnma->indexLastPrn19KeyThread[index];
					rxOsnma->wnLastPrn19Key[index]=rxOsnma->wnLastPrn19KeyThread[index];/*GST WN of the last PRN1 key saved*/
					rxOsnma->towLastPrn19Key[index]=rxOsnma->towLastPrn19KeyThread[index];/*GST TOW of the last PRN1 key saved*/
				}
				*(teslaKeyVerificationThFuncParam.chOsnma->keyStatusPtr) = VALIDATED;

				SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The thread for TESLA key verification finished with a successful validation!");
				
				/* validation log (TESLA Key) */
				if(rxOsnma->validationLogFile!=NULL)
				{
					fprintf(rxOsnma->validationLogFile,"%2d  %4d  %6d  %4d  %6d  %3d  %2d  %2d  %4d  %6d  %9d  %2d  %3d  %2d  %d\n",LOG_FLAG_TESLA_KEY_VALIDATION,wnMackStart,towMackStart+pars->mackBlockDuration,-1,-1,prnA,-1,-1,-1,-1,indexKey,-1,-1,-1,1);
				}
				
				return SUCCESS_RETURN;
			}
			else if(rxOsnma->indexLastKeyVerifiedThread!=-1)
			{
				*(teslaKeyVerificationThFuncParam.chOsnma->keyStatusPtr) = NOT_VALID;

				SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"The thread for TESLA key verification finished with a failed validation");
				
				/* validation log (TESLA Key) */
				if(rxOsnma->validationLogFile!=NULL)
				{
					fprintf(rxOsnma->validationLogFile,"%2d  %4d  %6d  %4d  %6d  %3d  %2d  %2d  %4d  %6d  %9d  %2d  %3d  %2d  %d\n",LOG_FLAG_TESLA_KEY_VALIDATION,wnMackStart,towMackStart+pars->mackBlockDuration,-1,-1,prnA,-1,-1,-1,-1,indexKey,-1,-1,-1,0);
				}
				
				return FAILURE_RETURN;
			}
		}
	}

	return FAILURE_RETURN;
}


void SaveMackInMackList(TOsnmaData *rxOsnma,TChannelOsnmaVariables *chOsnma,int wn,int tow)
{
	TOsnmaParameters *pars = &(rxOsnma->osnmaPars);
	TMacLut *macLut = &(pars->macLut);
	TMacLutSequence *seq;
	int blockIdx;
	int macIdx;
	int byteIdx;
	unsigned char *mac;
	unsigned char *key;
	unsigned char macInfo[2];
	unsigned char macseq[MACSEQ_SIZE_BYTES];
	unsigned char iod;
	TSatConstellation satType;
	int prn;
	int prnA = GetOsnmaPrnFromSatTypePrn(chOsnma->satType,chOsnma->prn);
	TAdkd adkd;
	TMackList *mackPtr;
	TMacList *macPtr, *macPtrToSaveMacInfo;
	//TKeyList *keyPtr;
	TMackList **mackPtrForDbg;
	TMacList **macPtrForDbg;
	int wnSubframeStart;
	int towSubframeStart;
	int wnMackStart;
	int towMackStart;
	int deltaTime;
	/* these indexes are initialized as the previous byte has already been parsed */
	int startByte = 0;
	int endByte = -1;
	int bitsOfFirstByte = 0;
	int bitsOfLastByte = BITS_PER_BYTE;
	int indexMacInfoFlex;
	int sctIdx;
	int indexKey;
	//**
	int TESLAKeyVerificationFlag;
	double timeError;
	int threadTESLAKeyVerificationIsRunningFlagToRead;


	/* if the OSNMA flag is Do not use, do not save MACK */
	if(chOsnma->nmaStatus==NMA_DO_NOT_USE)
	{
		return;
	}


	/* at the end of this function, some new MACs can be verified! */
	rxOsnma->newMacsAvailable = TRUE_FLAG;

	/* Allocate memory */
	mac = (unsigned char *)malloc(pars->macSizeBytes*sizeof(unsigned char));
	key = (unsigned char *)malloc((pars->keySize/BITS_PER_BYTE)*sizeof(unsigned char));
	mackPtrForDbg = (TMackList **)malloc(pars->macPerMackBlock*sizeof(TMackList *));
	macPtrForDbg = (TMacList **)malloc(pars->macPerMackBlock*sizeof(TMacList *));

	/* Correct time values in case of MACK offset
	   If the current MACK section is delayed with MACK Offset, then it started in the PREVIOUS subframe! */
	#ifdef MACK_OFFSET_ICD2_DEFINITION
	if(IsThisPrnOffset(prnA,pars->mackOffset,pars->ns)==TRUE_FLAG)
	#else
	if((pars->mackOffset==TRUE_FLAG) && (prnA>18))
	#endif
	{
		tow -= GAL_SECONDS_PER_SUBFRAME;
		if(tow<0)
		{
			wn--;
			tow += SECONDS_PER_WEEK;
		}
	}

	/* initialize time values */
	wnSubframeStart = wn;
	towSubframeStart = tow;
	wnMackStart = wn; /* start of first mack block =  start of subframe*/
	towMackStart = tow; /* start of first mack block = start of subframe */

	/* MAC0 */
	UpdateByteIndexBitIndex(&startByte,&endByte,&bitsOfFirstByte,&bitsOfLastByte,pars->macSize);
	ExtractBits(startByte,endByte,bitsOfFirstByte,bitsOfLastByte,chOsnma->chMack,mac);
	UpdateByteIndexBitIndex(&startByte,&endByte,&bitsOfFirstByte,&bitsOfLastByte,2*BITS_PER_BYTE);
	ExtractBits(startByte,endByte,bitsOfFirstByte,bitsOfLastByte,chOsnma->chMack,macInfo);
	macseq[0] = macInfo[0];
	macseq[1] = macInfo[1] & 0xf0;
	iod = macInfo[1] & 0x0f;
	adkd = ADKD_EPH_CLK_HEALTH; /* MAC0 has always ADKD==0 */
	mackPtr = PushInMackList(rxOsnma->mackList,chOsnma->satType,chOsnma->satSignal,chOsnma->prn,chOsnma->chId);
	if(rxOsnma->mackList==NULL)
	{
		/* list was empty */
		rxOsnma->mackList = mackPtr;
	}
	mackPtr->prnA = prnA;
	mackPtr->prnN = prnA;
	macPtr = PushNewMac(pars,mackPtr);
	macPtr->wnSubframeStart = wnSubframeStart;
	macPtr->towSubframeStart = towSubframeStart;
	macPtr->wnMackStart = wnMackStart;
	macPtr->towMackStart = towMackStart;
	macPtr->wnKey = wnMackStart;
	macPtr->towKey = towMackStart;
	//#ifdef OSNMA_EXTRA_KEY_DELAY
	if(pars->mackBlockOffset==1)
	{
		macPtr->towKey += pars->mackBlockDuration;
		if(macPtr->towKey>=SECONDS_PER_WEEK)
		{
			macPtr->towKey -= SECONDS_PER_WEEK;
			macPtr->wnKey++;
		}
	}
	/* for MAC0, NO SLOW MAC is possible! */
	macPtr->ctr = 1; /* MAC0 is always the first of the MACK block */
	memcpy(macPtr->mac,mac,pars->macSizeBytes);
	memcpy(macPtr->macseq,macseq,MACSEQ_SIZE_BYTES);
	/**/
	macPtrToSaveMacInfo=macPtr;
	ArrayClean((char *)macPtr->macInfoFlex,MAX_NUMBER_MACINFO_FLEX*MACINFO_SIZE_BYTES);
	indexMacInfoFlex=0;
	macPtrToSaveMacInfo->macInfoFlexSizeBytes=0;
	if(macLut->lutValid==TRUE_FLAG)
	{
		if(macLut->sct>1 && towSubframeStart%60!=0)
		{
			sctIdx=1;
		}
		else
		{
			sctIdx=0;
		}
		seq = &(macLut->sequences[sctIdx]);
	}
	else
	{
		seq = NULL;
	}
	/**/
	macPtr->adkd = adkd;
	macPtr->iod = iod;
	/* save pointers for debug */
	mackPtrForDbg[0] = mackPtr;
	macPtrForDbg[0] = macPtr;


	/* load all the MACs and key for all the MACK blocks */
	blockIdx = 0;
	//*********Prepare for MAC0 verification*********************
	macPtr->macToBeVerified=TRUE_FLAG;
	//*******************************
	macIdx = 1; /* MAC0 already parsed! */
	while(blockIdx<pars->nmack)
	{
		while(macIdx<pars->macPerMackBlock)
		{
			UpdateByteIndexBitIndex(&startByte,&endByte,&bitsOfFirstByte,&bitsOfLastByte,pars->macSize);
			ExtractBits(startByte,endByte,bitsOfFirstByte,bitsOfLastByte,chOsnma->chMack,mac);
			UpdateByteIndexBitIndex(&startByte,&endByte,&bitsOfFirstByte,&bitsOfLastByte,2*BITS_PER_BYTE);
			ExtractBits(startByte,endByte,bitsOfFirstByte,bitsOfLastByte,chOsnma->chMack,macInfo);
			GetSatTypePrnFromOsnmaPrn((int)macInfo[0],&satType,&prn); // macInfo[0] is the PRN (in OSNMA format)
			//**
			if(macLut->lutValid==TRUE_FLAG)
            {
				if((seq->flexLen>0) && (seq->sequence[(macLut->macsPerBlock*blockIdx)+macIdx] == MAC_LUT_FLEX))
				{
					memcpy(&macPtrToSaveMacInfo->macInfoFlex[indexMacInfoFlex*MACINFO_SIZE_BYTES],macInfo,MACINFO_SIZE_BYTES);
					indexMacInfoFlex++;
					macPtrToSaveMacInfo->macInfoFlexSizeBytes+=MACINFO_SIZE_BYTES;
				}
			}
			//**
			adkd = macInfo[1] >> 4;
			iod = macInfo[1] & 0x0f;
			if((prn!=INVALID_PRN) && (satType!=INVALID_SAT))
			{
				if(satType==GPS_SAT)
				{
					mackPtr = PushInMackList(rxOsnma->mackList,satType,L1_CA,prn,chOsnma->chId);
				}
				else if(adkd==ADKD_OTHER_NAV_MSG)
				{
					mackPtr = PushInMackList(rxOsnma->mackList,satType,E5_A,prn,chOsnma->chId); // F/NAV
				}
				else
				{
					mackPtr = PushInMackList(rxOsnma->mackList,satType,E1_BC,prn,chOsnma->chId);
				}
				mackPtr->prnA = prnA;
				mackPtr->prnN = (int)macInfo[0]; // macInfo[0] is the PRN (in OSNMA format)
				macPtr = PushNewMac(pars,mackPtr);
				macPtr->wnSubframeStart = wnSubframeStart;
				macPtr->towSubframeStart = towSubframeStart;
				macPtr->wnMackStart = wnMackStart;
				macPtr->towMackStart = towMackStart;
				macPtr->wnKey = wnMackStart;
				macPtr->towKey = towMackStart;
				//#ifdef OSNMA_EXTRA_KEY_DELAY
				if(pars->mackBlockOffset==1)
				{
					macPtr->towKey += pars->mackBlockDuration;
					if(macPtr->towKey>=SECONDS_PER_WEEK)
					{
						macPtr->towKey -= SECONDS_PER_WEEK;
						macPtr->wnKey++;
					}
				}
				if(adkd==ADKD_SLMAC_1_EXTRA)
				{
					macPtr->towKey += GAL_SECONDS_PER_SUBFRAME;
				}
				else if(adkd==ADKD_SLMAC_10_EXTRA)
				{
					macPtr->towKey += (10*GAL_SECONDS_PER_SUBFRAME);
				}
				if(macPtr->towKey>=SECONDS_PER_WEEK)
				{
					macPtr->towKey -= SECONDS_PER_WEEK;
					macPtr->wnKey++;
				}
				macPtr->ctr = (unsigned char)macIdx + 1; // +1, because CTR start from 1 whereas the loop index starts from 0
				memcpy(macPtr->mac,mac,pars->macSizeBytes);
				macPtr->adkd = adkd;
				macPtr->iod = iod;
				/* save pointers for debug */
				mackPtrForDbg[macIdx] = mackPtr;
				macPtrForDbg[macIdx] = macPtr;
				/*Prepare for MAC verification*/
				macPtr->macToBeVerified=TRUE_FLAG;
			}
			else
			{
				/* save pointers for debug:
				 * set to NULL if the MAC is not parsed (the log will not be saved) */
				mackPtrForDbg[macIdx] = NULL;
				macPtrForDbg[macIdx] = NULL;
			}
			/* next MAC */
			macIdx++;
		}
		/* Key */
		UpdateByteIndexBitIndex(&startByte,&endByte,&bitsOfFirstByte,&bitsOfLastByte,pars->keySize);
		ExtractBits(startByte,endByte,bitsOfFirstByte,bitsOfLastByte,chOsnma->chMack,key);
		mackPtr = PushInMackList(rxOsnma->mackList,chOsnma->satType,chOsnma->satSignal,chOsnma->prn,chOsnma->chId);
//		keyPtr = PushNewKey(pars,mackPtr);
//		keyPtr->wnSubframeStart = wnSubframeStart;
//		keyPtr->towSubframeStart = towSubframeStart;
//		keyPtr->wnKey = wnMackStart;
//		keyPtr->towKey = towMackStart;
		deltaTime = ((wnSubframeStart - pars->krootWn)*SECONDS_PER_WEEK) + (towSubframeStart - pars->krootTow);
//		keyPtr->indexKey  = (deltaTime/GAL_SECONDS_PER_SUBFRAME)*pars->nmack*pars->ns; // keys in the past complete subframes
//		keyPtr->indexKey += (blockIdx*pars->ns); // keys in the past MACK blocks (in the current subframe)
//		keyPtr->indexKey += (pars->keyIndexTable[chOsnma->prn -1]); // key of the current PRN
		indexKey  = (deltaTime/GAL_SECONDS_PER_SUBFRAME)*pars->nmack*pars->ns; // keys in the past complete subframes
		indexKey += (blockIdx*pars->ns); // keys in the past MACK blocks (in the current subframe)
		indexKey += (pars->keyIndexTable[chOsnma->prn -1]); // key of the current PRN
		//memcpy(keyPtr->key,key,pars->keySize/BITS_PER_BYTE);

		/* If the TESLA root key is available (i.e. indexLastKeyVerified>=0)
		 * proceed with the TESLA Chain key verification */
		if(rxOsnma->indexLastKeyVerified!=-1)
		{
			pthread_mutex_lock(&(rxOsnma->TESLAKeyVerificationMutex));
			threadTESLAKeyVerificationIsRunningFlagToRead=threadTESLAKeyVerificationStopIsRunning;
			pthread_mutex_unlock(&(rxOsnma->TESLAKeyVerificationMutex));
		
			/*If the thread is not running or has finished to run*/
			if(threadTESLAKeyVerificationIsRunningFlagToRead==FALSE_FLAG)
			{
				/* Check the presence of results from a terminated thread for the verification of TESLA keys*/
				CheckTeslaValidationThread(rxOsnma);

				/* Find the discrepancy with the current receiver clock emulated (if available) */
				timeError = 0;
				if(rxOsnma->receiverClockTow>0)
				{
					timeError = fabs((double)((rxOsnma->receiverClockWn - wn)*SECONDS_PER_WEEK) + (rxOsnma->receiverClockTow - tow));
					// time from receive refers to the end of the page,
					// whereas the time in the navigation message refers to the begin of the page
					timeError -= GAL_SECONDS_PER_PAGE;
				}
				if(timeError > (rxOsnma->receiverClockError))
				{
					/* if the time in the navigation message is not coherent with the emulated receiver time
					 * discard the TESLA chain key, since a meaconing attack is suspected */
					*(chOsnma->keyStatusPtr) = NOT_VALID;
					SwrxLog(NGENE_LOG_SEVERITY_WARNING,"TESLA key %d (PRN %2d, TOW %d) NOT verified (discarded due to a time discrepancy)",indexKey,prnA,tow);
					
					/* validation log (TESLA Key) */
					if(rxOsnma->validationLogFile!=NULL)
					{
						fprintf(rxOsnma->validationLogFile,"%2d  %4d  %6d  %4d  %6d  %3d  %2d  %2d  %4d  %6d  %9d  %2d  %3d  %2d  %d\n",LOG_FLAG_TESLA_KEY_VALIDATION,wnMackStart,towMackStart+pars->mackBlockDuration,-1,-1,prnA,-1,-1,-1,-1,indexKey,-1,-1,-1,0);
					}
				}
				/* The time is seemingly OK (or the check is not available). Standard validation. */
				else 
				{
					if(abs(indexKey-rxOsnma->indexLastKeyVerified)<=MAX_NUMBER_OF_TESLA_CHAIN_STEPS)
					{
						#ifdef OSNMA_PROFILING
						StartProfilingUpdate(&(rxOsnma->teslaProfiling));
						#endif
					    TESLAKeyVerificationFlag=TESLAKeyVerification(rxOsnma,key,prnA,indexKey,rxOsnma->indexLastKeyVerified,0,blockIdx);
						#ifdef OSNMA_PROFILING
					    StopProfilingUpdate(&(rxOsnma->teslaProfiling),1);
						#endif
						if(TESLAKeyVerificationFlag==SUCCESS_RETURN)
						{
							*(chOsnma->keyStatusPtr) = VALIDATED;
							if(indexKey>rxOsnma->indexLastKeyVerified)
							{
								rxOsnma->indexLastKeyVerified=indexKey;
								memcpy(rxOsnma->lastKeyVerified,key,pars->keySize/BITS_PER_BYTE);
							}
							
							/* validation log (TESLA Key) */
							if(rxOsnma->validationLogFile!=NULL)
							{
								fprintf(rxOsnma->validationLogFile,"%2d  %4d  %6d  %4d  %6d  %3d  %2d  %2d  %4d  %6d  %9d  %2d  %3d  %2d  %d\n",LOG_FLAG_TESLA_KEY_VALIDATION,wnMackStart,towMackStart+pars->mackBlockDuration,-1,-1,prnA,-1,-1,-1,-1,indexKey,-1,-1,-1,1);
							}
						}
						else
						{
							*(chOsnma->keyStatusPtr) = NOT_VALID;
							
							/* validation log (TESLA Key) */
							if(rxOsnma->validationLogFile!=NULL)
							{
								fprintf(rxOsnma->validationLogFile,"%2d  %4d  %6d  %4d  %6d  %3d  %2d  %2d  %4d  %6d  %9d  %2d  %3d  %2d  %d\n",LOG_FLAG_TESLA_KEY_VALIDATION,wnMackStart,towMackStart+pars->mackBlockDuration,-1,-1,prnA,-1,-1,-1,-1,indexKey,-1,-1,-1,0);
							}
						}
						
					}
					else
					{
						/*Initialize the structure to be used by the new thread*/
						InitTESLAKeyVerificationThreadFunction(&teslaKeyVerificationThFuncParam,rxOsnma, key,prnA,wnMackStart,towMackStart,indexKey,blockIdx);
						/*Create the thread to perform the remaining TESLA chain steps in parallel*/
						rxOsnma->keyVerificationStatusThread=VALIDATION_PENDING;
						teslaKeyVerificationThFuncParam.keyVerificationStatus = TO_BE_VALIDATED;
						teslaKeyVerificationThFuncParam.chOsnma = chOsnma;
						threadTESLAKeyVerificationStopIsRunning=TRUE_FLAG;
                        pthread_create(&threadTESLAKeyVerification, NULL, (void *)TESLAKeyVerificationThreadFunction, (void *) &teslaKeyVerificationThFuncParam);
						SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"Launched a new thread for TESLA key verification!!!");
					}
				}
			}
			else
			{
				SwrxLog(NGENE_LOG_SEVERITY_INFO,"TESLA key %d verification skipped (a validation thread is already running)",indexKey);
			}
		}

		//************************************

		/* Next MAC block */
		towMackStart += pars->mackBlockDuration;
		/* indexes point to the last byte of the previous MACK block */
		startByte = 0;
		endByte = ((blockIdx+1)*pars->mackBlockSizeBytes) -1;
		bitsOfFirstByte = 0;
		bitsOfLastByte = BITS_PER_BYTE;
		/* debug log */
		/* MAC */
		for(macIdx=0;macIdx<pars->macPerMackBlock;macIdx++)
		{
			if((mackPtrForDbg[macIdx]!=NULL) && (macPtrForDbg[macIdx]!=NULL) && (rxOsnma->macLogFile!=NULL))
			{
				/* if a pointer is NULL, skip! */
				fprintf(rxOsnma->macLogFile,"%4d ",wnSubframeStart);
				fprintf(rxOsnma->macLogFile,"%7d ",towSubframeStart);
				fprintf(rxOsnma->macLogFile,"%7d ",macPtrForDbg[macIdx]->towMackStart);
				fprintf(rxOsnma->macLogFile,"%3d ",mackPtrForDbg[macIdx]->prnN);
				fprintf(rxOsnma->macLogFile,"%3d ",mackPtrForDbg[macIdx]->prnA);
				fprintf(rxOsnma->macLogFile,"%2d ",macPtrForDbg[macIdx]->adkd);
				fprintf(rxOsnma->macLogFile,"%2u ",macPtrForDbg[macIdx]->iod);
				fprintf(rxOsnma->macLogFile,"%2d ",blockIdx+1);
				fprintf(rxOsnma->macLogFile,"%2d ",macIdx+1);
				PrintABitString(rxOsnma->macLogFile,macPtrForDbg[macIdx]->mac,pars->macSize);
				fprintf(rxOsnma->macLogFile,"    ");
				fprintf(rxOsnma->macLogFile,"%7d  ",indexKey);
				for(byteIdx=0;byteIdx<(pars->keySize/BITS_PER_BYTE);byteIdx++)
				{
					fprintf(rxOsnma->macLogFile,"%4u",key[byteIdx]);
				}
				fprintf(rxOsnma->macLogFile,"\n");
				fflush(rxOsnma->macLogFile);
			}
		}
		/* MACSEQ */
		if((blockIdx==0) && (rxOsnma->macseqLogFile!=NULL))
		{
			/* only MAC0 contains MACSEQ */
			fprintf(rxOsnma->macseqLogFile,"%4d ",wnSubframeStart);
			fprintf(rxOsnma->macseqLogFile,"%7d ",towSubframeStart);
			fprintf(rxOsnma->macseqLogFile,"%3d      ",mackPtrForDbg[0]->prnA);
			PrintABitString(rxOsnma->macseqLogFile,macseq,MACSEQ_SIZE_BITS);
			fprintf(rxOsnma->macseqLogFile,"\n");
		}
		/* update indexes */
		/* first MAC of next block */
		macIdx = 0;
		blockIdx++;
	}

	free(mac);
	free(key);
	free(mackPtrForDbg);
	free(macPtrForDbg);
}


void SaveNavdataInMackList(TOsnmaData *rxOsnma, TChannelOsnmaVariables *chOsnma,int wn,int tow)
{
	TMackList *mackPtr;
	TNavDataList *navDataPtr;

	mackPtr = PushInMackList(rxOsnma->mackList,chOsnma->satType,chOsnma->satSignal,chOsnma->prn,chOsnma->chId);
	if(rxOsnma->mackList==NULL)
	{
		/* list was empty */
		rxOsnma->mackList = mackPtr;
	}
	navDataPtr = PushNewNavData(mackPtr);
	navDataPtr->wnSubframeStart = wn;
	navDataPtr->towSubframeStart = tow;
	memcpy(navDataPtr->wordTypes,chOsnma->chWordTypes,GAL_PAGES_PER_SUBFRAME*sizeof(int));
	memcpy(navDataPtr->navData,chOsnma->chSubframe,GAL_BYTES_PER_SUBFRAME);
}

#ifndef PATCH_IGNORE_IOD_IN_MAC_AUTHENTICATION
int ExtractAndCheckBitsForMacVerification(TOsnmaData *rxOsnma,TSatConstellation satType,TSatSignal satSignal,int prn,int chId,int wn,int tow,TAdkd adkd,unsigned int iodMac,TNavDataList **navDataCurrPtr,TNavDataList **navDataPrevPtr)
{
	int returnValue;
	unsigned int iodNav;
	int oldestWn;
	int oldestTow;
	int searchFinished = FALSE_FLAG;

	/* init navdata outputs for navdata log */
	*navDataCurrPtr = NULL;
	*navDataPrevPtr = NULL;

	/* get the time for the oldest subframe available */
	if(GetOldestTowInNavDataList(rxOsnma->mackList,satType,satSignal,prn,chId,&oldestWn,&oldestTow)==FAILURE_RETURN)
	{
		// no subframes available!
		return FAILURE_RETURN;
	}

	/* Preliminary adaptation of TOW and WN as a function of ADKD.
	 *
	 * When ADKD=5, the duration of the subframe F/NAV is 50 s,
	 * which is not a multiple of the duration of a subframe I/NAV (30 s).
	 **/
	if(adkd==ADKD_OTHER_NAV_MSG)
	{
		tow = ((int)tow/FNAV_SUBFRAME_DURATION)*FNAV_SUBFRAME_DURATION;
	}

	/* Get the navigation data */
	returnValue = ExtractBitsForMacVerification(rxOsnma,satType,satSignal,prn,chId,wn,tow,adkd,iodMac,navDataCurrPtr,navDataPrevPtr);
	if((returnValue==FAILURE_RETURN) &&
	   (adkd!=ADKD_EPH_CLK_HEALTH) && (adkd!=ADKD_SLMAC_1_EXTRA) && (adkd!=ADKD_SLMAC_10_EXTRA) && (adkd!=ADKD_OTHER_NAV_MSG))
	{
		/* when the desired subframe is not found:
		 * - if the MAC authenticates ephemerides try with an older subframe
		 *   (the same stands also for F/NAV)
		 * - for other MACs, terminate here */
		return returnValue;
	}

	/* check consistency; when the check fails, if possible, try to recover the correct data */
	switch(adkd)
	{
		case ADKD_EPH_CLK_HEALTH:
		case ADKD_SLMAC_1_EXTRA:
		case ADKD_SLMAC_10_EXTRA:
			/* check the consistency between IODs */
			iodMac = iodMac & 0x07;
			if(returnValue==SUCCESS_RETURN)
			{
				if(satType==GALILEO_SAT)
				{
					// first 10 bits of navdataBitsForMac are IODnav from Word1; get 3 LSBs
					iodNav = ((rxOsnma->navdataBitsForMac[0] & 0x01) << 2) | ((rxOsnma->navdataBitsForMac[1] >> 6) & 0x03);
				}
				else // GPS_SAT
				{
					// take from subframe 2
					iodNav = ((rxOsnma->navdataBitsForMac[11] & 0x01) << 2) | ((rxOsnma->navdataBitsForMac[12] >> 6) & 0x03);
				}
			}
			else
			{
				// not available, set to a value different from iodMac to start a search
				iodNav = iodMac+1;
			}
			// if IOD differ, try with an older subframe!
			while((searchFinished==FALSE_FLAG) && (iodNav!=iodMac))
			{
				// start of the previous subframe
				tow -= GAL_SECONDS_PER_SUBFRAME;
				if(tow<0)
				{
					wn--;
					tow += SECONDS_PER_WEEK;
				}
				if((wn<oldestWn) || ((wn==oldestWn) && (tow<oldestTow)))
				{
					// not available, terminate
					searchFinished = TRUE_FLAG;
					returnValue = FAILURE_RETURN;
				}
				else
				{
					// extract again bits!
					returnValue = ExtractBitsForMacVerification(rxOsnma,satType,satSignal,prn,chId,wn,tow,adkd,iodMac,navDataCurrPtr,navDataPrevPtr);
					// update iodNav (when returnValue is SUCCESS)
					if(returnValue==SUCCESS_RETURN)
					{
						if(satType==GALILEO_SAT)
						{
							iodNav = ((rxOsnma->navdataBitsForMac[0] & 0x01) << 2) | ((rxOsnma->navdataBitsForMac[1] >> 6) & 0x03);
						}
						else // GPS_SAT
						{
							iodNav = ((rxOsnma->navdataBitsForMac[11] & 0x01) << 2) | ((rxOsnma->navdataBitsForMac[12] >> 6) & 0x03);
						}
					}
				}
			}
			break;
		case ADKD_OTHER_NAV_MSG:
			/* check the consistency between IODs */
			iodMac = iodMac & 0x07;
			if(returnValue==SUCCESS_RETURN)
			{
				#ifdef ADKD_5_ICD2_DEFINITION
				// skip 6 bits SVID, the following 10 bits of navdataBitsForMac are IODnav from Page1; get 3 LSBs
				iodNav = rxOsnma->navdataBitsForMac[1] & 0x07;
				#else
				// skip 6 bits Type, 6 bits SVID, the following 10 bits of navdataBitsForMac are IODnav from Page1; get 3 LSBs
				iodNav = (rxOsnma->navdataBitsForMac[2] & 0x1c) >> 2;
				#endif
			}
			else
			{
				// not available, set to a value different from iodMac to start a search
				iodNav = iodMac+1;
			}
			// if IOD differ, try with an older subframe!
			while((searchFinished==FALSE_FLAG) && (iodNav!=iodMac))
			{
				// start of the previous subframe
				tow -= FNAV_SUBFRAME_DURATION;
				if(tow<0)
				{
					wn--;
					tow += SECONDS_PER_WEEK;
				}
				if((wn<oldestWn) || ((wn==oldestWn) && (tow<oldestTow)))
				{
					// not available, terminate
					searchFinished = TRUE_FLAG;
					returnValue = FAILURE_RETURN;
				}
				else
				{
					// extract again bits!
					returnValue = ExtractBitsForMacVerification(rxOsnma,satType,satSignal,prn,chId,wn,tow,adkd,iodMac,navDataCurrPtr,navDataPrevPtr);
					// update iodNav (when returnValue is SUCCESS)
					if(returnValue==SUCCESS_RETURN)
					{
						iodNav = rxOsnma->navdataBitsForMac[1] & 0x07;
					}
				}
			}
			break;
		default:
			/* nothing to do (maybe) */
			break;
	}

	return returnValue;
}


int ExtractBitsForMacVerification(TOsnmaData *rxOsnma,TSatConstellation satType,TSatSignal satSignal,int prn,int chId,int wn,int tow,TAdkd adkd,unsigned int iodMac,TNavDataList **navDataCurrPtr,TNavDataList **navDataPrevPtr)
{
	TMackList *mackPtr;
	TNavDataList *navDataCurr;
	TNavDataList *navDataPrev = NULL;
	int *wordTypesOld = NULL;
	unsigned char *navDataOld = NULL;

	rxOsnma->navdataBitsForMacLen = -1;

	mackPtr = FindInMackList(rxOsnma->mackList,satType,satSignal,prn,chId);
	if(mackPtr==NULL)
	{
		/* no information available for the required PRN */
		return FAILURE_RETURN;
	}
	if(mackPtr->navDataList==NULL)
	{
		/* no navigation data available for the required PRN */
		return FAILURE_RETURN;
	}
	navDataCurr = FindNavData(mackPtr->navDataList,wn,tow);
	if(navDataCurr==NULL)
	{
		/* required subframe not present */
		return FAILURE_RETURN;
	}
	/* Manage the alignment between MACs and navigation data
	 * Manage additional subframes needed for some ADKDs
	 * NB ADKD!=0 are not supported for GPS! */
	#ifdef PATCH_ADKD_NAVDATA_ALIGNMENT
	#ifdef PATCH_ADKD_4_NAVDATA_ALIGNMENT
	// enable  (adkd==ADKD_GTS_UTC_GST_GPS) when required by the elaborated dataset (e.g. TV213 from JRC)
	if((adkd==ADKD_GAL_SUBF_GPS_FRAME) || (adkd==ADKD_ALMANAC) || (adkd==ADKD_GTS_UTC_GST_GPS))
	#else
	if((adkd==ADKD_GAL_SUBF_GPS_FRAME) || (adkd==ADKD_ALMANAC))
	#endif
	{
		if(navDataCurr->prevNavData==NULL)
		{
			/* the previous subframe, needed for ADKD_ALMANAC and ADKD_GTS_UTC_GST_GPS is not available */
			return FAILURE_RETURN;
		}
		if(navDataCurr->prevNavData->prevNavData==NULL)
		{
			/* the previous subframe, needed for ADKD_ALMANAC and ADKD_GTS_UTC_GST_GPS is not available */
			return FAILURE_RETURN;
		}
		if((navDataCurr->prevNavData->towSubframeStart!=tow-GAL_SECONDS_PER_SUBFRAME) ||
		   (navDataCurr->prevNavData->prevNavData->towSubframeStart!=tow-(2*GAL_SECONDS_PER_SUBFRAME)))
		{
			/* the time difference between the three subframes is wrong */
			return FAILURE_RETURN;
		}
		/* I need two subframes before the current one */
		navDataCurr = navDataCurr->prevNavData;
		navDataPrev = navDataCurr->prevNavData;
		wordTypesOld = navDataPrev->wordTypes;
		navDataOld = navDataPrev->navData;
	}
	#else
	if(adkd==ADKD_ALMANAC)
	{
		if(navDataCurr->prevNavData==NULL)
		{
			/* the previous subframe, needed for ADKD_ALMANAC and ADKD_GTS_UTC_GST_GPS is not available */
			return FAILURE_RETURN;
		}

		if(navDataCurr->prevNavData->towSubframeStart!=tow-GAL_SECONDS_PER_SUBFRAME)
		{
			/* the time difference between the three subframes is wrong */
			return FAILURE_RETURN;
		}
		/* I need one subframe in addition to the current one */
		navDataPrev = navDataCurr->prevNavData;
		wordTypesOld = navDataPrev->wordTypes;
		navDataOld = navDataPrev->navData;
	}
	#endif
	else if(adkd==ADKD_GTS_UTC_GST_GPS)
	{
		navDataPrev = navDataCurr->prevNavData;
		if(navDataPrev==NULL)
		{
			/* the previous subframe, needed for ADKD_ALMANAC and ADKD_GTS_UTC_GST_GPS is not available */
			return FAILURE_RETURN;
		}
		if(navDataPrev->towSubframeStart!=tow-GAL_SECONDS_PER_SUBFRAME)
		{
			/* the time difference between the two subframes is wrong */
			return FAILURE_RETURN;
		}
		wordTypesOld = navDataPrev->wordTypes;
		navDataOld = navDataPrev->navData;
	}
	else
	{
		/* ADKD 0, 11, 12, 5
		 * if the New Data flag (in MAC's iod) is zero, it is safer to use the previous subframe,
		 * to avoid problems when the complete IOD changes, but the truncated IOD remains the same */
		if(((iodMac & 0x08)==0) && (navDataCurr->prevNavData!=NULL))
		{
			navDataCurr = navDataCurr->prevNavData;
		}
		wordTypesOld = NULL;
		navDataOld = NULL;
	}
	/* output navData pointers */
	*navDataCurrPtr = navDataCurr;
	*navDataPrevPtr = navDataPrev;
	/* Extract the bits from the selected subframes */
	ArrayClean((char *)rxOsnma->navdataBitsForMac,MAX_NAVDATA_BYTES_FOR_MAC_VERIFICATION);
	return (ExtractNavDataBits(satType,adkd,navDataOld,wordTypesOld,navDataCurr->navData,navDataCurr->wordTypes,rxOsnma->navdataBitsForMac,&(rxOsnma->navdataBitsForMacLen)));
}
#else
// if PATCH_IGNORE_IOD_IN_MAC_AUTHENTICATION is defined
int ExtractAndCheckBitsForMacVerification(TOsnmaData *rxOsnma,TSatConstellation satType,TSatSignal satSignal,int prn,int chId,int wn,int tow,TAdkd adkd,unsigned int *iodMac,TNavDataList **navDataCurrPtr,TNavDataList **navDataPrevPtr)
{
	int returnValue;

	/* init navdata outputs for navdata log */
	*navDataCurrPtr = NULL;
	*navDataPrevPtr = NULL;

	/* Preliminary adaptation of TOW and WN as a function of ADKD.
	 *
	 * When ADKD=5, the duration of the subframe F/NAV is 50 s,
	 * which is not a multiple of the duration of a subframe I/NAV (30 s).
	 **/
	if(adkd==ADKD_OTHER_NAV_MSG)
	{
		tow = ((int)tow/FNAV_SUBFRAME_DURATION)*FNAV_SUBFRAME_DURATION;
	}

	/* Get the navigation data
	 * ALWAYS use the previous subframe: if not available, skip */
	returnValue = ExtractBitsForMacVerification(rxOsnma,satType,satSignal,prn,chId,wn,tow,adkd,navDataCurrPtr,navDataPrevPtr);
	if(returnValue==FAILURE_RETURN)
	{
		return returnValue;
	}

	/* if needed, overwrite iodMac with the value found in the navigation message */
	switch(adkd)
	{
		case ADKD_EPH_CLK_HEALTH:
		case ADKD_SLMAC_1_EXTRA:
		case ADKD_SLMAC_10_EXTRA:
			if(satType==GALILEO_SAT)
			{
				// first 10 bits of navdataBitsForMac are IODnav from Word1; get 3 LSBs
				*iodMac = ((rxOsnma->navdataBitsForMac[0] & 0x01) << 2) | ((rxOsnma->navdataBitsForMac[1] >> 6) & 0x03);
			}
			else // GPS_SAT
			{
				// take from subframe 2
				*iodMac = ((rxOsnma->navdataBitsForMac[11] & 0x01) << 2) | ((rxOsnma->navdataBitsForMac[12] >> 6) & 0x03);
			}
			break;
		case ADKD_OTHER_NAV_MSG:
			#ifdef ADKD_5_ICD2_DEFINITION
			// skip 6 bits SVID, the following 10 bits of navdataBitsForMac are IODnav from Page1; get 3 LSBs
			*iodMac = rxOsnma->navdataBitsForMac[1] & 0x07;
			#else
			// skip 6 bits Type, 6 bits SVID, the following 10 bits of navdataBitsForMac are IODnav from Page1; get 3 LSBs
			*iodMac = (rxOsnma->navdataBitsForMac[2] & 0x1c) >> 2;
			#endif
			break;
		default:
			/* nothing to do (maybe) */
			break;
	}

	return returnValue;
}


int ExtractBitsForMacVerification(TOsnmaData *rxOsnma,TSatConstellation satType,TSatSignal satSignal,int prn,int chId,int wn,int tow,TAdkd adkd,TNavDataList **navDataCurrPtr,TNavDataList **navDataPrevPtr)
{
	TMackList *mackPtr;
	TNavDataList *navDataCurr;
	TNavDataList *navDataPrev = NULL;
	int *wordTypesOld = NULL;
	unsigned char *navDataOld = NULL;

	rxOsnma->navdataBitsForMacLen = -1;

	mackPtr = FindInMackList(rxOsnma->mackList,satType,satSignal,prn,chId);
	if(mackPtr==NULL)
	{
		/* no information available for the required PRN */
		return FAILURE_RETURN;
	}
	if(mackPtr->navDataList==NULL)
	{
		/* no navigation data available for the required PRN */
		return FAILURE_RETURN;
	}

	/* Manage the alignment between MACs and navigation data
	 *
	 * The MAC is ALWAYS associated to the navigation data transmitted in the previous subframe
	 * Manage additional subframes needed for some ADKDs
	 * NB ADKD!=0 are not supported for GPS!
	 */

	/* Adjust TOW for the previous subframe */
	if(adkd==ADKD_OTHER_NAV_MSG)
	{
		tow -= FNAV_SUBFRAME_DURATION;
	}
	else
	{
		tow -= GAL_SECONDS_PER_SUBFRAME;
	}

	/* Find navigation data for the current subframe */
	navDataCurr = FindNavData(mackPtr->navDataList,wn,tow);
	if(navDataCurr==NULL)
	{
		// required subframe not present
		return FAILURE_RETURN;
	}
	/* ... and the previous subframe */
	navDataPrev = navDataCurr->prevNavData;

	/* Manage different ADKDs */
	switch(adkd)
	{
	case ADKD_EPH_CLK_HEALTH:
	case ADKD_OTHER_NAV_MSG:
	case ADKD_SLMAC_1_EXTRA:
	case ADKD_SLMAC_10_EXTRA:
	case ADKD_GAL_SUBF_GPS_FRAME:
		// only current subframe is needed
		wordTypesOld = NULL;
		navDataOld = NULL;
		break;
	case ADKD_ALMANAC:
	case ADKD_GTS_UTC_GST_GPS:
		if(navDataPrev==NULL)
		{
			/* the previous subframe is not available */
			return FAILURE_RETURN;
		}
		if(navDataPrev->towSubframeStart!=tow-GAL_SECONDS_PER_SUBFRAME)
		{
			/* the time difference between the subframes is wrong */
			return FAILURE_RETURN;
		}
		wordTypesOld = navDataPrev->wordTypes;
		navDataOld = navDataPrev->navData;
		break;
	default:
		/* not supported */
		return FAILURE_RETURN;
		break;
	}

	/* output navData pointers */
	*navDataCurrPtr = navDataCurr;
	*navDataPrevPtr = navDataPrev;
	/* Extract the bits from the selected subframes */
	ArrayClean((char *)rxOsnma->navdataBitsForMac,MAX_NAVDATA_BYTES_FOR_MAC_VERIFICATION);
	return (ExtractNavDataBits(satType,adkd,navDataOld,wordTypesOld,navDataCurr->navData,navDataCurr->wordTypes,rxOsnma->navdataBitsForMac,&(rxOsnma->navdataBitsForMacLen)));
}
#endif


int ExtractNavDataBits(TSatConstellation satType,TAdkd adkd,unsigned char *navData1,int *wordTypes1,unsigned char *navData2,int *wordTypes2,unsigned char *arrayOut,int *len)
{
	int byteIdxIn;
	int bitIdxIn;
	int byteIdxOut = 0;
	int bitIdxOut = 0;
	int wordIdx;
	/* for almanac only */
	unsigned char *word07;
	unsigned char *word08;
	unsigned char *word09;
	unsigned char *word10;

	/* clean output array */
	ArrayClean((char *)arrayOut,MAX_NAVDATA_BYTES_FOR_MAC_VERIFICATION);
	/* number of extracted bits is set to zero */
	*len = 0;

	/* check input parameters */
	if(navData2==NULL)
	{
		return FAILURE_RETURN;
	}
	if((adkd==ADKD_ALMANAC) && ((navData1==NULL) || (wordTypes1==NULL) || ((wordTypes2==NULL))))
	{
		/* ADKD_ALMANAC needs both the current and the previous subframes
		 * and both wordTypes */
		return FAILURE_RETURN;
	}
	if((adkd==ADKD_GTS_UTC_GST_GPS) && ((navData1==NULL) || (wordTypes1==NULL) || ((wordTypes2==NULL))))
	{
		/* ADKD_GTS_UTC_GST_GPS needs word10, which could be stored in the previous subframe */
		return FAILURE_RETURN;
	}

	/* For Galileo:
	 * input data format: navData2 stores all the bits from a subframe of the navigation message
	 * 120 bits (in 15 bytes) for the even part and 120 bits (15 bytes) for the odd part of each page
	 * in the reception order, i.e.
	 *               index of the first byte
	 * word 2 even    0*15
	 * word 2 odd     1*15
	 * word 4 even    2*15
	 * word 4 odd     3*15
	 *
	 * and so on...
	 * The same stand for navData1 */

	/* For F/NAV:
	 * input data format: navData2 stores all the bits from a subframe of the navigation message
	 * 244 bits for page, i.e. 31 bytes included 4 bits padding at the end of each page.
	 *          index of the first byte
	 * page 1    0*31
	 * page 2    1*31
	 *
	 * and so on...
	 * The same stand for navData1 */

	if(satType==GALILEO_SAT)
	{
		/* copy bits in functions of the current ADKD */
		switch(adkd)
		{
		case ADKD_EPH_CLK_HEALTH:
		case ADKD_SLMAC_1_EXTRA:
		case ADKD_SLMAC_10_EXTRA:
			/* ADKD0 and Slow MACs (ADKD11 and 12) are managed in the same way
			 * only the time of the associated key changes  */
			/* word 1 */
			/* even part, skip first byte: even/odd (1 bit),page type (1 bit) and word type (6 bits) */
			byteIdxIn = (20*GAL_DECODED_PAGE_PART_LENGTH_BYTES) +1;
			bitIdxIn = 0;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,106);
			/* odd part, skip 2 bits: even/odd (1 bit),page type (1 bit) */
			byteIdxIn = (21*GAL_DECODED_PAGE_PART_LENGTH_BYTES);
			bitIdxIn = 2;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,14);
			/* word 2 */
			/* even part, skip first byte: even/odd (1 bit),page type (1 bit) and word type (6 bits) */
			byteIdxIn = ( 0*GAL_DECODED_PAGE_PART_LENGTH_BYTES) +1;
			bitIdxIn = 0;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,106);
			/* odd part, skip 2 bits: even/odd (1 bit),page type (1 bit) */
			byteIdxIn = ( 1*GAL_DECODED_PAGE_PART_LENGTH_BYTES);
			bitIdxIn = 2;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,14);
			/* word 3 */
			/* even part, skip first byte: even/odd (1 bit),page type (1 bit) and word type (6 bits) */
			byteIdxIn = (22*GAL_DECODED_PAGE_PART_LENGTH_BYTES) +1;
			bitIdxIn = 0;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,106);
			/* odd part, skip 2 bits: even/odd (1 bit),page type (1 bit) */
			byteIdxIn = (23*GAL_DECODED_PAGE_PART_LENGTH_BYTES);
			bitIdxIn = 2;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,16);
			/* word 4 even part, skip first byte: even/odd (1 bit),page type (1 bit) and word type (6 bits) */
			byteIdxIn = ( 2*GAL_DECODED_PAGE_PART_LENGTH_BYTES) +1;
			bitIdxIn = 0;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,106);
			/* odd part, skip 2 bits: even/odd (1 bit),page type (1 bit) */
			byteIdxIn = ( 3*GAL_DECODED_PAGE_PART_LENGTH_BYTES);
			bitIdxIn = 2;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,14);
			/* word 5 */
			/* even part, skip first byte: even/odd (1 bit),page type (1 bit) and word type (6 bits) */
			byteIdxIn = (24*GAL_DECODED_PAGE_PART_LENGTH_BYTES) +1;
			bitIdxIn = 0;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,67);
			/* update length */
			*len = NAVDATA_BITS_FOR_GAL_ADKD_00;
			break;

		case ADKD_GAL_SUBF_GPS_FRAME:
			byteIdxIn = 0;
			bitIdxIn = 1; /* skip Even/odd bit */
			for(wordIdx=0;wordIdx<GAL_PAGES_PER_SUBFRAME;wordIdx++)
			{
				if((wordTypes2[wordIdx]==0) || (wordTypes2[wordIdx]==63))
				{
					/* Spare words and Dummy pages are managed differently */
					/* Page type and Word Type from even part */
					CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,7);
					(*len) += 7;
					byteIdxIn += GAL_DECODED_PAGE_PART_LENGTH_BYTES;
					/* TODO check if Page type is saved from both even and odd parts */
					/* Page Type from odd part */
					#ifndef PATCH_ADKD_2_BIT_MASK
					CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,1);
					(*len) += 1;
					#endif
					byteIdxIn += GAL_DECODED_PAGE_PART_LENGTH_BYTES;
				}
				else
				{
					/* Page Type and Data (1/2) from even part */
					CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,113);
					byteIdxIn += GAL_DECODED_PAGE_PART_LENGTH_BYTES;
					/* Page Type and Data (2/2) from odd part */
					CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,17);
					byteIdxIn += GAL_DECODED_PAGE_PART_LENGTH_BYTES;
					(*len) += 130;
				}
			}
			break;

		case ADKD_ALMANAC:
			/* check almanac words */
			if(wordTypes2[3]==7)
			{
				/* words 7 and  8 from navData2,
				 * words 9 and 10 from navData1 */
				word07 = &(navData2[6*GAL_DECODED_PAGE_PART_LENGTH_BYTES]);
				word08 = &(navData2[8*GAL_DECODED_PAGE_PART_LENGTH_BYTES]);
				word09 = &(navData1[6*GAL_DECODED_PAGE_PART_LENGTH_BYTES]);
				word10 = &(navData1[8*GAL_DECODED_PAGE_PART_LENGTH_BYTES]);
			}
			else
			{
				/* words 7 and  8 from navData1,
				 * words 9 and 10 from navData2 */
				word07 = &(navData1[6*GAL_DECODED_PAGE_PART_LENGTH_BYTES]);
				word08 = &(navData1[8*GAL_DECODED_PAGE_PART_LENGTH_BYTES]);
				word09 = &(navData2[6*GAL_DECODED_PAGE_PART_LENGTH_BYTES]);
				word10 = &(navData2[8*GAL_DECODED_PAGE_PART_LENGTH_BYTES]);
			}
			/* word 7 */
			/* even part, skip first byte: even/odd (1 bit),page type (1 bit) and word type (6 bits) */
			byteIdxIn = 1;
			bitIdxIn = 0;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,word07,&byteIdxOut,&bitIdxOut,arrayOut,106);
			/* odd part, skip 2 bits: even/odd (1 bit),page type (1 bit) */
			byteIdxIn = GAL_DECODED_PAGE_PART_LENGTH_BYTES;
			bitIdxIn = 2;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,word07,&byteIdxOut,&bitIdxOut,arrayOut,10);
			/* word 8 */
			/* even part, skip first byte: even/odd (1 bit),page type (1 bit) and word type (6 bits) */
			byteIdxIn = 1;
			bitIdxIn = 0;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,word08,&byteIdxOut,&bitIdxOut,arrayOut,106);
			/* odd part, skip 2 bits: even/odd (1 bit),page type (1 bit) */
			byteIdxIn = GAL_DECODED_PAGE_PART_LENGTH_BYTES;
			bitIdxIn = 2;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,word08,&byteIdxOut,&bitIdxOut,arrayOut,15);
			/* word 9 */
			/* even part, skip first byte: even/odd (1 bit),page type (1 bit) and word type (6 bits) */
			byteIdxIn = 1;
			bitIdxIn = 0;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,word09,&byteIdxOut,&bitIdxOut,arrayOut,106);
			/* odd part, skip 2 bits: even/odd (1 bit),page type (1 bit) */
			byteIdxIn = GAL_DECODED_PAGE_PART_LENGTH_BYTES;
			bitIdxIn = 2;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,word09,&byteIdxOut,&bitIdxOut,arrayOut,16);
			/* word 10 */
			/* even part, skip first byte: even/odd (1 bit),page type (1 bit) and word type (6 bits) */
			byteIdxIn = 1;
			bitIdxIn = 0;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,word10,&byteIdxOut,&bitIdxOut,arrayOut,80);
			/* update length */
			*len = NAVDATA_BITS_FOR_GAL_ADKD_03;
			break;

		case ADKD_GTS_UTC_GST_GPS:
			/* check word10 */
			if(wordTypes1[4]==10)
			{
				word10 = &(navData1[8*GAL_DECODED_PAGE_PART_LENGTH_BYTES]);
			}
			else
			{
				word10 = &(navData2[8*GAL_DECODED_PAGE_PART_LENGTH_BYTES]);
			}
			/* word 6 */
			/* even part, skip first byte: even/odd (1 bit),page type (1 bit) and word type (6 bits) */
			byteIdxIn = ( 4*GAL_DECODED_PAGE_PART_LENGTH_BYTES) +1;
			bitIdxIn = 0;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,106);
			/* odd part, skip 2 bits: even/odd (1 bit),page type (1 bit) */
			byteIdxIn = ( 5*GAL_DECODED_PAGE_PART_LENGTH_BYTES);
			bitIdxIn = 2;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,13);
			/* word 10 */
			/* even part, skip 88 bits (even/odd, page type, word type, almanac) */
			byteIdxIn = 11;
			bitIdxIn = 0;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,word10,&byteIdxOut,&bitIdxOut,arrayOut,26);
			/* odd part, skip 2 bits: even/odd (1 bit),page type (1 bit) */
			byteIdxIn = GAL_DECODED_PAGE_PART_LENGTH_BYTES;
			bitIdxIn = 2;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,word10,&byteIdxOut,&bitIdxOut,arrayOut,16);
			/* update length */
			*len = NAVDATA_BITS_FOR_GAL_ADKD_04;
			break;

		case ADKD_OTHER_NAV_MSG:
			#ifdef ADKD_5_ICD2_DEFINITION
			/* page 1 */
			/* skip preamble (6 bits) */
			byteIdxIn = 0;
			bitIdxIn = 6;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,149);
			/* E5aDVS */
			byteIdxIn = 23;
			bitIdxIn = 3;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,1);
			/* page 2 */
			/* skip preamble (6 bits) */
			byteIdxIn = 1*FNAV_DECODED_PAGE_LENGTH_BYTES;
			bitIdxIn = 6;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,176);
			/* page 3 */
			/* skip preamble (6 bits) */
			byteIdxIn = 2*FNAV_DECODED_PAGE_LENGTH_BYTES;
			bitIdxIn = 6;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,168);
			/* page 4 */
			/* skip preamble (6 bits) */
			byteIdxIn = 3*FNAV_DECODED_PAGE_LENGTH_BYTES;
			bitIdxIn = 6;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,183);
			/* update length */
			*len = NAVDATA_BITS_FOR_FNAV_ADKD_05;
			#else
			/* page 1 */
			byteIdxIn = 0;
			bitIdxIn = 0;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,149+6);
			/* E5aDVS */
			byteIdxIn = 23;
			bitIdxIn = 3;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,1);
			/* page 2 */
			byteIdxIn = 1*FNAV_DECODED_PAGE_LENGTH_BYTES;
			bitIdxIn = 0;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,176+6);
			/* page 3 */
			byteIdxIn = 2*FNAV_DECODED_PAGE_LENGTH_BYTES;
			bitIdxIn = 0;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,168+6);
			/* page 4 */
			byteIdxIn = 3*FNAV_DECODED_PAGE_LENGTH_BYTES;
			bitIdxIn = 0;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,183+6);
			/* update length */
			*len = NAVDATA_BITS_FOR_FNAV_ADKD_05;
			#endif
			break;

		/* other cases not supported yet */
		default:
			*len = 0;
			return FAILURE_RETURN;
			break;
		}
	}


	/*
	 * For GPS:
	 * the bits are saved, including the repetition of the two last parity bits of the previous word, i.e.
	 * 1 word = 32 bits => parity bit + parity bit + 30 bits
	 * So each subframe uses 320 bits (instead of 300 bits).
	 * Five consecutive subframes are saved in each navdata structure.
	 */
	if(satType==GPS_SAT)
	{
		/* copy bits in functions of the current ADKD */
		switch(adkd)
		{
		case ADKD_EPH_CLK_HEALTH:
			/* subframe 1 */
			/* URA, SV health, IODC (2 MSB) */
			byteIdxIn = (0*GPS_SUBFRAME_BYTES) +9;
			bitIdxIn = 6;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,12);
			/* TGD */
			byteIdxIn = (0*GPS_SUBFRAME_BYTES) +26;
			bitIdxIn = 2;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,8);
			/* IODC (8 LSB), Toc */
			byteIdxIn = (0*GPS_SUBFRAME_BYTES) +28;
			bitIdxIn = 2;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,24);
			/* af2, af1 */
			byteIdxIn = (0*GPS_SUBFRAME_BYTES) +32;
			bitIdxIn = 2;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,24);
			/* af0 */
			byteIdxIn = (0*GPS_SUBFRAME_BYTES) +36;
			bitIdxIn = 2;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,22);
			/* subframe 2 */
			/* IODE, Crs */
			byteIdxIn = (1*GPS_SUBFRAME_BYTES) +8;
			bitIdxIn = 2;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,24);
			/* deltaN, M0 (MSB) */
			byteIdxIn = (1*GPS_SUBFRAME_BYTES) +12;
			bitIdxIn = 2;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,24);
			/* M0 (LSB) */
			byteIdxIn = (1*GPS_SUBFRAME_BYTES) +16;
			bitIdxIn = 2;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,24);
			/* Cuc, e (MSB) */
			byteIdxIn = (1*GPS_SUBFRAME_BYTES) +20;
			bitIdxIn = 2;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,24);
			/* e (LSB) */
			byteIdxIn = (1*GPS_SUBFRAME_BYTES) +24;
			bitIdxIn = 2;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,24);
			/* Cus, sqrtA (MSB) */
			byteIdxIn = (1*GPS_SUBFRAME_BYTES) +28;
			bitIdxIn = 2;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,24);
			/* sqrtA (LSB) */
			byteIdxIn = (1*GPS_SUBFRAME_BYTES) +32;
			bitIdxIn = 2;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,24);
			/* toe, fit interval flag, AODO */
			byteIdxIn = (1*GPS_SUBFRAME_BYTES) +36;
			bitIdxIn = 2;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,22);
			/* subframe 3 */
			/* Cic, Omega0 (MSB) */
			byteIdxIn = (2*GPS_SUBFRAME_BYTES) +8;
			bitIdxIn = 2;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,24);
			/* Omega0 (LSB) */
			byteIdxIn = (2*GPS_SUBFRAME_BYTES) +12;
			bitIdxIn = 2;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,24);
			/* Cis, i0 (MSB) */
			byteIdxIn = (2*GPS_SUBFRAME_BYTES) +16;
			bitIdxIn = 2;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,24);
			/* i0 (LSB) */
			byteIdxIn = (2*GPS_SUBFRAME_BYTES) +20;
			bitIdxIn = 2;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,24);
			/* Crc, omega (MSB) */
			byteIdxIn = (2*GPS_SUBFRAME_BYTES) +24;
			bitIdxIn = 2;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,24);
			/* omega (LSB) */
			byteIdxIn = (2*GPS_SUBFRAME_BYTES) +28;
			bitIdxIn = 2;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,24);
			/* omega dot */
			byteIdxIn = (2*GPS_SUBFRAME_BYTES) +32;
			bitIdxIn = 2;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,24);
			/* iode, idot */
			byteIdxIn = (2*GPS_SUBFRAME_BYTES) +36;
			bitIdxIn = 2;
			CopyBitsUnaligned(byteIdxIn,bitIdxIn,navData2,&byteIdxOut,&bitIdxOut,arrayOut,22);
			/* update length */
			*len = NAVDATA_BITS_FOR_GPS_ADKD_00;
			break;

		/* other cases not supported yet */
		default:
			*len = 0;
			return FAILURE_RETURN;
			break;
		}
	}

	return SUCCESS_RETURN;
}



void SaveDsmBlock(TOsnmaData *rxOsnma, TChannelOsnmaVariables *chOsnma,int wn,int tow)
{
	TOsnmaParameters *pars = &(rxOsnma->osnmaPars);
	TDsmData *dsmData = &(rxOsnma->dsmData);
	TDsmArray *dsmElement;
	int dsmid = chOsnma->chDsm[1]>>4;
	int bid = chOsnma->chDsm[1] & 0x0f;
	int dsmDestIdx;
	int idx;
	int npkt;

	/* DSM log */
	if(rxOsnma->dsmLogFile!=NULL)
	{
		fprintf(rxOsnma->dsmLogFile,"%4d %7d  %2d     ",wn,tow,chOsnma->prn);
		// NMA header
		fprintf(rxOsnma->dsmLogFile,"%2d %2d %2d     ",chOsnma->chDsm[0]>>6,(chOsnma->chDsm[0] & 0x30)>>4,(chOsnma->chDsm[0] & 0x0e)>>1); // NMA Status, Chain ID, Chain & PK status
		// DSM header
		fprintf(rxOsnma->dsmLogFile,"%2d %2d     ",dsmid,bid);
		// DSM bytes
		for(idx=2;idx<HKROOT_PER_SUBFRAME_SIZE;idx++)
		{
			fprintf(rxOsnma->dsmLogFile,"%3d ",chOsnma->chDsm[idx]);
		}
		fprintf(rxOsnma->dsmLogFile,"\n");
	}
	/**/

	/* if dsmId changed, then update the structure */
	if(dsmid != dsmData->currDsmId)
	{
		dsmData->currDsmId = dsmid;
		dsmData->dsmType = GetDsmTypeFromDsmId(dsmid);
		dsmData->currNb = (dsmData->dsmType==DSM_PKR) ? pars->dsmPkrNb : pars->dsmKrootNb; // not used until now (2019/10/08)
		dsmData->wnDsmStart = wn;   // not used until now (2019/10/08)
		dsmData->towDsmStart = tow; // not used until now (2019/10/08)
		ClearDsmValidityAll(dsmData);
	}
	/* DSM_KROOT: check if NB is already initialized */
	if((dsmData->dsmType==DSM_KROOT) && (pars->dsmKrootNb<0) && (bid==0x00))
	{
		pars->dsmKrootNb = GetNumberOfBlocks(chOsnma->chDsm[2]>>4);
		dsmData->currNb = pars->dsmKrootNb;
	}
	/* DSM_PKR: set parameters when available */
	if((dsmData->dsmType==DSM_PKR) && (bid==0x00))
	{
		/* DSM-PKR Nb. of Blocks and Message ID are in the third byte of BID 0 */
		pars->dsmPkrNb = GetNumberOfBlocks(chOsnma->chDsm[2]>>4);
		dsmData->currNb = pars->dsmPkrNb;
		pars->dsmPkrMessageId = chOsnma->chDsm[2] & 0x0f;
	}
	if((dsmData->dsmType==DSM_PKR) && (bid==0x09))
	{
		/* DSM-PKR New Public Key Type and New Public Key ID the last byte of BID 9 */
		npkt = chOsnma->chDsm[HKROOT_PER_SUBFRAME_SIZE-1]>>4;
		pars->dsmPkrNpkt = (npkt>NPKT_RSVD) ?  NPKT_RSVD : npkt;
		pars->dsmPkrNpkId = chOsnma->chDsm[HKROOT_PER_SUBFRAME_SIZE-1] & 0x0f;
		// get NPK length (bits)
		switch(pars->dsmPkrNpkt)
		{
			case ECDSA_P224:
				rxOsnma->pkrNpkLen = ECDSA_COMPRESSED_KEY_LEN_P224;
				break;
			case ECDSA_P256:
				rxOsnma->pkrNpkLen = ECDSA_COMPRESSED_KEY_LEN_P256;
				break;
			case ECDSA_P384:
				rxOsnma->pkrNpkLen = ECDSA_COMPRESSED_KEY_LEN_P384;
				break;
			case ECDSA_P521:
				rxOsnma->pkrNpkLen = ECDSA_COMPRESSED_KEY_LEN_P521;
				break;
			case EMERGENCY_SERVICE_MESSAGE:
				rxOsnma->pkrNpkLen = ECDSA_COMPRESSED_KEY_LEN_ESM;
				break;
			case NPKT_RSVD:
				rxOsnma->pkrNpkLen = -1;
				break;
			default:
				rxOsnma->pkrNpkLen = -1;
				break;
		}
	}

	/* now, save DSM information! */
	dsmDestIdx = bid*DSM_BLOCK_SIZE; // destination index
	// copy in the array common for all the PRNs (if not present yet)
	if(dsmData->validity[bid]==FALSE_FLAG)
	{
		memcpy(&(dsmData->dsmBlocks[dsmDestIdx]),&chOsnma->chDsm[2],DSM_BLOCK_SIZE);
		dsmData->validity[bid] = TRUE_FLAG;
	}
	// copy in the array specific for the current PRN
	dsmElement = &(dsmData->dsmArray[chOsnma->chId]);
	dsmElement->satId = chOsnma->prn;
	dsmElement->satType = chOsnma->satType;
	memcpy(&(dsmElement->dsmBlocks[dsmDestIdx]),&chOsnma->chDsm[2],DSM_BLOCK_SIZE);
	dsmElement->validity[bid] = TRUE_FLAG;
}


void DeleteChannelFromOsnmaData(TOsnmaData *rxOsnma,int chId)
{
	/* MACK */
	rxOsnma->mackList = DeleteChannelFromMackList(rxOsnma->mackList,chId);
	/* DSM */
	ClearDsmValidityOneChannel(&(rxOsnma->dsmData),chId);
}
