/**
 * @file managePvtConfiguration.c
 *
 * @author Mario Nicola
 *
 * @date 07/08/2014
 *
 * @version 1.0.0
 *
 * @brief Functions that manage PVT parameters.
 */

#include <string.h>
#include <stdlib.h>
#include "pvtConfiguration.h"
#include "ngeneConstants.h"
#include "ioFunctions.h"
#include "closeManager.h"

int ManagePvtConfiguration(TNGeneConfiguration *cfg)
{
	TPvtConfiguration *pvtCfg;
	char cfgFilename[MAX_ROW_LENGTH];
	char tag[MAX_TAG_LEN];

	pvtCfg = &(cfg->pvtConfiguration);
	cfgFilename[0] = '\0';
	strcat(cfgFilename, cfg->configurationPath);
	strcat(cfgFilename, "PVT.conf");

	strcpy(tag, "PVT rate");
	if(GetVal(cfgFilename,tag,(void *)&(pvtCfg->pvtRate),F_FLOAT,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	/**/
	strcpy(tag, "Max GDOP");
	if(GetVal(cfgFilename,tag,(void *)&(pvtCfg->maxGdop),F_FLOAT,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	/**/
	strcpy(tag, "PVT method");
	if(GetVal(cfgFilename,tag,(void *)&(pvtCfg->pvtMethod),F_INTEGER,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	if((pvtCfg->pvtMethod!=LMS_BASED_PVT) && (pvtCfg->pvtMethod!=KALMAN_BASED_PVT))
	{
		pvtCfg->pvtMethod=LMS_BASED_PVT;
	}
	/**/
	strcpy(tag, "CSM filter length");
	if(GetVal(cfgFilename,tag,(void *)&(pvtCfg->csmFilterLen),F_INTEGER,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	/**/
	strcpy(tag, "Use elevation mask");
	if(GetVal(cfgFilename,tag,(void *)&(pvtCfg->elevationMaskFlag),F_INTEGER,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	if(pvtCfg->elevationMaskFlag==TRUE_FLAG)
	{
		strcpy(tag, "Elevation mask");
		if(GetVal(cfgFilename,tag,(void *)&(pvtCfg->elevationMask),F_FLOAT,1) == FAILURE_RETURN)
		{
			SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
			CloseNGene(cfg,EXIT_FAILURE);
		}
		pvtCfg->elevationMask *= DEGREE_TO_RADIAN;
	}
	else
	{
		pvtCfg->elevationMaskFlag = FALSE_FLAG;
		pvtCfg->elevationMask = 0.0;
	}
	/**/
	strcpy(tag, "Use urban canyon mask");
	if(GetVal(cfgFilename,tag,(void *)&(pvtCfg->urbanCanyonMaskFlag),F_INTEGER,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	/**/
	strcpy(tag, "Use EGNOS");
	if(GetVal(cfgFilename,tag,(void *)&(pvtCfg->egnosFlag),F_INTEGER,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	if(pvtCfg->egnosFlag!=TRUE_FLAG)
	{
		pvtCfg->egnosFlag=FALSE_FLAG;
	}
	/**/
	strcpy(tag, "Static Receiver");
	if(GetVal(cfgFilename,tag,(void *)&(pvtCfg->staticReceiver),F_INTEGER,1) == FAILURE_RETURN)
	{
		// if the parameter is not found in the file, the default value is TRUE_FLAG
		pvtCfg->staticReceiver = TRUE_FLAG;
	}
	if(pvtCfg->staticReceiver!=FALSE_FLAG)
	{
		pvtCfg->staticReceiver=TRUE_FLAG;
	}

	/* Use of authentication */
	strcpy(tag, "Not Authenticated Sats in PVT");
	if(GetVal(cfgFilename,tag,(void *)&(pvtCfg->pvtAuthSel),F_INTEGER,1) == FAILURE_RETURN)
	{
		pvtCfg->pvtAuthSel = PVT_DO_NOT_DISCARD;
		SwrxLog(NGENE_LOG_SEVERITY_WARNING,"Tag <%s> not found in <%s>. Using default value %d",tag,cfgFilename,pvtCfg->pvtAuthSel);
	}
	if((pvtCfg->pvtAuthSel!=PVT_DISCARD_NOT_AUTHENTICATED) &&
	   (pvtCfg->pvtAuthSel!=PVT_DISCARD_AUTHENTICATION_FAILURES) &&
	   (pvtCfg->pvtAuthSel!=PVT_DO_NOT_DISCARD))
	{
		pvtCfg->pvtAuthSel = PVT_DO_NOT_DISCARD;
		SwrxLog(NGENE_LOG_SEVERITY_WARNING,"Tag <%s> with invalid value in <%s>. Using default value %d",tag,cfgFilename,pvtCfg->pvtAuthSel);
	}

	/* Use of RedCED */
	strcpy(tag, "Enable RedCED in PVT");
	if(GetVal(cfgFilename,tag,(void *)&(pvtCfg->redCedInPvt),F_INTEGER,1) == FAILURE_RETURN)
	{
		pvtCfg->redCedInPvt = RED_CED_ENABLED;
		SwrxLog(NGENE_LOG_SEVERITY_WARNING,"Tag <%s> not found in <%s>. Using default value %d",tag,cfgFilename,pvtCfg->redCedInPvt);
	}
	if((pvtCfg->redCedInPvt!=RED_CED_DISABLED) &&
	   (pvtCfg->redCedInPvt!=RED_CED_ENABLED) &&
	   (pvtCfg->redCedInPvt!=RED_CED_ONLY))
	{
		pvtCfg->redCedInPvt = RED_CED_ENABLED;
		SwrxLog(NGENE_LOG_SEVERITY_WARNING,"Tag <%s> with invalid value in <%s>. Using default value %d",tag,cfgFilename,pvtCfg->redCedInPvt);
	}

	/* iono models */
	// Klobuchar
	pvtCfg->ionoDefaultValid = TRUE_FLAG;
	strcpy(tag, "Klobuchar alfa");
	if(GetVal(cfgFilename,tag,(void *)&(pvtCfg->ionoDefault.alphaGps),F_EXP,4) == FAILURE_RETURN)
	{
		pvtCfg->ionoDefaultValid = FALSE_FLAG;
	}
	strcpy(tag, "Klobuchar beta");
	if(GetVal(cfgFilename,tag,(void *)&(pvtCfg->ionoDefault.beta),F_DOUBLE,4) == FAILURE_RETURN)
	{
		pvtCfg->ionoDefaultValid = FALSE_FLAG;
	}
	// Nequick
	strcpy(tag, "Nequick alfa");
	if(GetVal(cfgFilename,tag,(void *)&(pvtCfg->ionoDefault.alphaGal),F_DOUBLE,3) == FAILURE_RETURN)
	{
		pvtCfg->ionoDefaultValid = FALSE_FLAG;
	}
	return SUCCESS_RETURN;
}



