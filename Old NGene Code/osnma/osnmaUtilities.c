/**
 * @file osnmaUtilities.c
 *
 * @author Mario Nicola
 *
 * @date 12/04/2019
 *
 * @version 1.0.0
 *
 * @brief Some easy functions to parse and manage OSNMA
 */

#include <math.h>
#include "osnmaUtilities.h"
#include "navigationMessageParser.h"
#include "ngeneConstants.h"
#include "timeConstants.h"
#include <openssl/sha.h>
#include <openssl/evp.h>
#include "osnmaConstants.h"


TDsmType GetDsmTypeFromDsmId(int dsmId)
{
	if((dsmId>=0) && (dsmId<=11))
	{
		return DSM_KROOT;
	}
	else if((dsmId>=12) && (dsmId<=15))
	{
		return DSM_PKR;
	}
	else
	{
		/* should not happen */
		return DSM_INVALID_TYPE;
	}
}


int GetNumberOfBlocks(int nb)
{
	/* nb=1 => 7  ...  nb=10 => 16 */
	if((nb>=1) && (nb<=10))
	{
		return (6+nb);
	}
	/* other values are reserved */
	return -1;
}


int GetTeslaChainKeySize(int ks)
{
	switch(ks)
	{
	case 0:
		return 96;
		break;
	case 1:
		return 104;
		break;
	case 2:
		return 112;
		break;
	case 3:
		return 120;
		break;
	case 4:
		return 128;
		break;
	case 5:
		return 160;
		break;
	case 6:
		return 192;
		break;
	case 7:
		return 224;
		break;
	case 8:
		return 256;
		break;
	default:
		return -1;
		break;
	}
}




void GetHashFunctionForTeslaKeyVerification(TOsnmaParameters *osnma)
{
	switch(osnma->hashFunction)
	{
	case SHA_256:
		osnma->function=EVP_sha256();
		osnma->functionThread=EVP_sha256();
		break;
	case SHA3_224:
		osnma->function=EVP_sha3_224();
		osnma->functionThread=EVP_sha3_224();
		break;
	case SHA3_256:
		osnma->function=EVP_sha3_256();
		osnma->functionThread=EVP_sha3_256();
		break;
	case HF_RSVD:
			/*rsv*/
		break;
	default:
		osnma->function=EVP_sha256();
		osnma->functionThread=EVP_sha256();
		break;
	}
}

int GetMacSize(int ms)
{
	switch(ms)
	{
	case 0:
		return 10;
		break;
	case 1:
		return 12;
		break;
	case 2:
		return 14;
		break;
	case 3:
		return 16;
		break;
	case 4:
		return 18;
		break;
	case 5:
		return 20;
		break;
	case 6:
		return 24;
		break;
	case 7:
		return 28;
		break;
	case 8:
		return 32;
		break;
	case 9:
		return 40;
		break;
	default:
		return -1;
		break;
	}
}


void GetSatTypePrnFromOsnmaPrn(int osnmaPrn,TSatConstellation *satTypePtr,int *prnPtr)
{
	if((osnmaPrn>=1) && (osnmaPrn<=36))
	{
		*satTypePtr = GALILEO_SAT;
		*prnPtr = osnmaPrn;
	}
	else if((osnmaPrn>=64) && (osnmaPrn<=95))
	{
		*satTypePtr = GPS_SAT;
		*prnPtr = osnmaPrn-63;
	}
	else if((osnmaPrn>=173) && (osnmaPrn<=190))
	{
		*satTypePtr = EGNOS_SAT;
		*prnPtr = osnmaPrn-53;
	}
	else if(osnmaPrn==OSNMA_GENERAL_GPS_INFO)
	{
		*satTypePtr = GPS_SAT;
		*prnPtr = OSNMA_GENERAL_GPS_INFO;
	}
	else if(osnmaPrn==OSNMA_GENERAL_GALILEO_INFO)
	{
		*satTypePtr = GALILEO_SAT;
		*prnPtr = OSNMA_GENERAL_GALILEO_INFO;
	}
	else
	{
		*satTypePtr = INVALID_SAT;
		*prnPtr = INVALID_PRN;
	}
}


int GetOsnmaPrnFromSatTypePrn(TSatConstellation satType,int prn)
{
	int osnmaPrn = 0;

	if(satType==GALILEO_SAT)
	{
		osnmaPrn = prn;
	}
	else if((satType==GPS_SAT) && (prn!=OSNMA_GENERAL_GPS_INFO))
	{
		osnmaPrn = prn + 63;
	}
	else if((satType==GPS_SAT) && (prn==OSNMA_GENERAL_GPS_INFO))
	{
		osnmaPrn = prn;
	}
	else if(satType==EGNOS_SAT)
	{
		osnmaPrn = prn + 53;
	}
	return osnmaPrn;
}


int IsThisPrnOffset(int prn,int mackOffsetEnabled,int ns)
{
	// MACK offset disabled
	if(mackOffsetEnabled==FALSE_FLAG)
		return FALSE_FLAG;

	// no MACK offset with NS==1
	if(ns==1)
		return FALSE_FLAG;

	// definition
	if((((prn-1)%ns)+1) <= (int)(ns/2))
		return FALSE_FLAG;
	else
		return TRUE_FLAG;
}


#ifdef MACK_OFFSET_ICD2_DEFINITION
double GetMackOffsetSeconds(int prn,int mackOffsetEnabled,int nmack,int ns)
{
	/* if MACK offset is disabled, return 0.0 */
	if(mackOffsetEnabled==FALSE_FLAG)
	{
		return 0.0;
	}
	/* use equation at page 42 (ICD 2.1) */
	if(IsThisPrnOffset(prn,mackOffsetEnabled,ns)==FALSE_FLAG)
	{
		return 0.0;
	}
	/* else offset is function of NMACK Table 19 (ICD 2.1) */
	switch(nmack)
	{
	case 1:
		return 16.0;
		break;
	case 2:
		return 8.0;
		break;
	case 3:
		return 4.0;
		break;
	default:
		/* not valid */
		return 0.0;
		break;
	}
}
#else
double GetMackOffsetSeconds(int prn,int mackOffsetEnabled,int nmack)
{
	/* if MACK offset is disabled, return 0.0 */
	if(mackOffsetEnabled==FALSE_FLAG)
	{
		return 0.0;
	}
	/* for PRNs from 1 to 18, no MACK offset is applied. See page 33 */
	if(prn<19)
	{
		return 0.0;
	}
	/* else offset is function of NMACK */
	switch(nmack)
	{
	case 1:
		return 18.0;
		break;
	case 2:
		return 8.0;
		break;
	case 3:
	    #ifdef PATCH_JRC_GENERATOR_MACK_OFFSET
		return 4.0; 
		#else
		return 6.0;
		#endif
		break;
	default:
		/* not valid */
		return 0.0;
		break;
	}
}
#endif


int GetGalE1bSubframeStartTime(int tow,int *offsetPtr)
{
	/* ICD at page 26 requires:
	 * Galileo System Time at the start of the 30-second interval in which the sub-frame
	 * in which the key K_m will be transmitted begins transmission.
	 * For E1, this is the E1 sub-frame start time minus 1 second. */
	tow = tow -1;
	/* Since pages start at odd time values (1,3,5,..,29 seconds and so on),
	 * an explicitly subtraction of 1 is not needed to find the sub-frame start.
	 * But the subtraction is necessary to correctly check the MACK offset. */
	*offsetPtr = tow%GAL_SECONDS_PER_SUBFRAME;
	return tow - (*offsetPtr);
}


int FromTeslaChainKeyIndexToPrn(int maxNs, int ns,int *keyIndexTable, int indexKey,int prnIndex)
{
	/* In each mack block there are 36/NS PRNs with the same key!
	 * prnIndex goes from 1 to ceiling(36/NS), then
	 *   prnIndex==1 => return the first PRN with this key index
	 *   prnIndex==2 => return the second PRN with this key index
	 * and so on... */
	int maxPrnIndex = ((int)ceil((double)maxNs)/((double)ns));
	int indexInTheMackBlock;
	int idx;
	int idxFound;
	int prn;

	/* Root key is not allocated to a PRN */
	if(indexKey==0)
	{
		return 0;
	}
	/* check validity of input (prnIndex not valid) */
	if((prnIndex>maxPrnIndex) || (prnIndex<1))
	{
		return INVALID_PRN;
	}
	indexInTheMackBlock = ((indexKey-1)%ns) +1; // it goes from 1 to ns
	/* look for the PRN */
	idxFound=0;
	idx=0;
	prn = INVALID_PRN;
	while((idxFound!=prnIndex) && (idx<maxNs))
	{
		if(keyIndexTable[idx]==indexInTheMackBlock)
		{
			idxFound++;
		}
		else
		{
			idx++;
		}
	}
	if(idxFound==prnIndex)
	{
		prn = idx+1;
	}
	return prn;
}

int FromTeslaChainKeyIndexToTime(int nmack, int ns, int krootTow, int indexKey)
{
	int subframeIndex = (indexKey-1)/(nmack*ns);
	int tow = krootTow + (subframeIndex*GAL_SECONDS_PER_SUBFRAME);

	while(tow>=SECONDS_PER_WEEK)
	{
		tow-=SECONDS_PER_WEEK;
	}
	return tow;
}


int IsThisKeyForMackOffset(int mackOffsetEnabled,int ns,int indexKey)
{
	int indexKeyInTheMackBlock;

	// MACK Offset disabled
	if(mackOffsetEnabled==FALSE_FLAG)
		return FALSE_FLAG;

	// no MACK offset with NS==1
	if(ns==1)
		return FALSE_FLAG;

	// indexKey starts from 0, the root key
	// get the index of the key inside the MACK Block (from 1 to NS)
	indexKeyInTheMackBlock = ((indexKey -1)%ns) +1;

	// apply the definition
	if(indexKeyInTheMackBlock==(1+(ns/2)))
		return TRUE_FLAG;
	else
		return FALSE_FLAG;
}
