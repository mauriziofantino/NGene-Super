/**
 * @file manageTrkConfiguration.c
 *
 * @author Mario Nicola
 *
 * @date 07/08/2014
 *
 * @version 1.0.0
 *
 * @brief Functions that manage tracking parameters.
 */

#include <string.h>
#include <stdlib.h>
#include "trkConfiguration.h"
#include "ngeneConstants.h"
#include "ioFunctions.h"
#include "closeManager.h"

int ManageTrkConfiguration(TNGeneConfiguration *cfg)
{
	TTrkConfiguration *trkCfg;
	char cfgFilename[MAX_ROW_LENGTH];
	char tag[MAX_TAG_LEN];

	trkCfg = &(cfg->trkConfiguration);
	cfgFilename[0] = '\0';
	strcat(cfgFilename, cfg->configurationPath);
	strcat(cfgFilename, "Tracking.conf");

	strcpy(tag, "CN0 cutoff");
	if(GetVal(cfgFilename,tag,(void *)&(trkCfg->cutoffCno),F_FLOAT,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	/**/
	strcpy(tag, "GPS userIntperiod");
	if(GetVal(cfgFilename,tag,(void *)&(trkCfg->gpsIntegrationPeriod),F_INTEGER,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	/**/
	strcpy(tag, "GPS DLLbandwidth");
	if(GetVal(cfgFilename,tag,(void *)&(trkCfg->gpsDllBw),F_FLOAT,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	/**/
	strcpy(tag, "GPS DLLgain");
	if(GetVal(cfgFilename,tag,(void *)&(trkCfg->gpsDllGain),F_FLOAT,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	/**/
	strcpy(tag, "GPS FLLbandwidth");
	if(GetVal(cfgFilename,tag,(void *)&(trkCfg->gpsFllBw),F_FLOAT,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	/**/
	strcpy(tag, "GPS FLLgain");
	if(GetVal(cfgFilename,tag,(void *)&(trkCfg->gpsFllGain),F_FLOAT,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	/**/
	strcpy(tag, "GPS PLLbandwidth");
	if(GetVal(cfgFilename,tag,(void *)&(trkCfg->gpsPllBw),F_FLOAT,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	/**/
	strcpy(tag, "GPS PLLgain");
	if(GetVal(cfgFilename,tag,(void *)&(trkCfg->gpsPllGain),F_FLOAT,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	/**/
	strcpy(tag, "GPS spacing");
	if(GetVal(cfgFilename,tag,(void *)&(trkCfg->gpsSpacing),F_FLOAT,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	/**/
	strcpy(tag, "EGNOS userIntperiod");
	if(GetVal(cfgFilename,tag,(void *)&(trkCfg->egnosIntegrationPeriod),F_INTEGER,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	trkCfg->egnosDllBw = trkCfg->gpsDllBw;
	trkCfg->egnosDllGain = trkCfg->gpsDllGain;
	trkCfg->egnosFllBw = trkCfg->gpsFllBw;
	trkCfg->egnosFllGain = trkCfg->gpsFllGain;
	trkCfg->egnosPllBw = trkCfg->gpsPllBw;
	trkCfg->egnosPllGain = trkCfg->gpsPllGain;
	trkCfg->egnosSpacing = trkCfg->gpsSpacing;
	/**/
	strcpy(tag, "Galileo DLLbandwidth");
	if(GetVal(cfgFilename,tag,(void *)&(trkCfg->galileoDllBw),F_FLOAT,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	/**/
	strcpy(tag, "Galileo DLLgain");
	if(GetVal(cfgFilename,tag,(void *)&(trkCfg->galileoDllGain),F_FLOAT,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	/**/
	strcpy(tag, "Galileo FLLbandwidth");
	if(GetVal(cfgFilename,tag,(void *)&(trkCfg->galileoFllBw),F_FLOAT,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	/**/
	strcpy(tag, "Galileo FLLgain");
	if(GetVal(cfgFilename,tag,(void *)&(trkCfg->galileoFllGain),F_FLOAT,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	/**/
	strcpy(tag, "Galileo PLLbandwidth");
	if(GetVal(cfgFilename,tag,(void *)&(trkCfg->galileoPllBw),F_FLOAT,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	/**/
	strcpy(tag, "Galileo PLLgain");
	if(GetVal(cfgFilename,tag,(void *)&(trkCfg->galileoPllGain),F_FLOAT,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	/**/
	strcpy(tag, "Galileo spacing");
	if(GetVal(cfgFilename,tag,(void *)&(trkCfg->galileoSpacing),F_FLOAT,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	/**/
	strcpy(tag, "FLLPLLChoiceFilter Length");
	if(GetVal(cfgFilename,tag,(void *)&(trkCfg->fllPllChoiceFilterLen),F_INTEGER,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	/**/
	strcpy(tag, "FLLPLLChoiceFilterUpdateTime");
	if(GetVal(cfgFilename,tag,(void *)&(trkCfg->fllPllChoiceFilterUpdateTime),F_INTEGER,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	/**/
	strcpy(tag, "PLLFLLthreshold");
	if(GetVal(cfgFilename,tag,(void *)&(trkCfg->fllPllThreshold),F_FLOAT,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	/**/
	strcpy(tag, "changePeriodCtrlLength");
	if(GetVal(cfgFilename,tag,(void *)&(trkCfg->changePeriodCtrlLength),F_INTEGER,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	/**/
	/**/
	strcpy(tag, "Very Early Very Late GPS spacing");
	if(GetVal(cfgFilename,tag,(void *)&(trkCfg->veryEarlyVeryLateGpsSpacing),F_FLOAT,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	strcpy(tag, "Very Early Very Late Galileo spacing");
	if(GetVal(cfgFilename,tag,(void *)&(trkCfg->veryEarlyVeryLateGalileoSpacing),F_FLOAT,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	trkCfg->veryEarlyVeryLateEgnosSpacing = trkCfg->veryEarlyVeryLateGpsSpacing;

	/**/
	strcpy(tag, "Multiple Correlators Flag");
	if(GetVal(cfgFilename,tag,(void *)&(trkCfg->multipleCorrelatorsFlag),F_INTEGER,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	strcpy(tag, "Number Of Multiple Correlators");
	if(GetVal(cfgFilename,tag,(void *)&(trkCfg->numberOfMultipleCorrelators),F_INTEGER,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	if(trkCfg->multipleCorrelatorsFlag==TRUE_FLAG)
	{
		if(trkCfg->numberOfMultipleCorrelators<3)
		{
			trkCfg->numberOfMultipleCorrelators = 3;
			SwrxLog(NGENE_LOG_SEVERITY_WARNING,"The minimum number of multiple correlators should be 3",trkCfg->numberOfMultipleCorrelators);
		}
		else if((trkCfg->numberOfMultipleCorrelators%2)==0)
		{
			trkCfg->numberOfMultipleCorrelators++;
			SwrxLog(NGENE_LOG_SEVERITY_WARNING,"The number of multiple correlators must be an odd number. Set to %d",trkCfg->numberOfMultipleCorrelators);
		}
	}
	strcpy(tag, "Multiple Correlators GPS Spacing");
	if(GetVal(cfgFilename,tag,(void *)&(trkCfg->multipleCorrelatorsGpsSpacing),F_INTEGER,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	strcpy(tag, "Multiple Correlators Galileo Spacing");
	if(GetVal(cfgFilename,tag,(void *)&(trkCfg->multipleCorrelatorsGalileoSpacing),F_INTEGER,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	strcpy(tag, "Multiple Correlators EGNOS Spacing");
	if(GetVal(cfgFilename,tag,(void *)&(trkCfg->multipleCorrelatorsEgnosSpacing),F_INTEGER,1) == FAILURE_RETURN)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Error while parsing configuration file <%s>: tag <%s> not found",cfgFilename,tag);
		CloseNGene(cfg,EXIT_FAILURE);
	}
	/**/

	return SUCCESS_RETURN;
}


