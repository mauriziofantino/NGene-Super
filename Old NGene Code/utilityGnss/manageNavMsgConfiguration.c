/**
 * @file manageNavMsgConfiguration.c
 *
 * @author Mario Nicola
 *
 * @author Mario Nicola
 *
 * @date 15/04/2019
 *
 * @version 1.0.0
 *
 * @brief Manage configuration parameters related to navigation message parsing.
 */

#include <string.h>
#include "ngeneConfiguration.h"
#include "navigationMessageConfiguration.h"
#include "ioFunctions.h"

int ManageNavigationMessageConfiguration(TNGeneConfiguration *cfg)
{
	TNavMsgConfiguration *navMsgCfg;
	char cfgFilename[MAX_ROW_LENGTH];
	char tag[MAX_TAG_LEN];

	navMsgCfg = &(cfg->navMsgConfiguration);
	cfgFilename[0] = '\0';
	strcat(cfgFilename, cfg->configurationPath);
	strcat(cfgFilename, "NavigationMessages.conf");

	/* WN correction */
	strcpy(tag, "GPS WN correction");
	if(GetVal(cfgFilename,tag,(void *)&(navMsgCfg->gpsWnCorrection),F_INTEGER,1) == FAILURE_RETURN)
	{
		/* use the default value */
		navMsgCfg->gpsWnCorrection = GPS_WEEK_NUMBER_CORRECTION;
	}
	else if(((navMsgCfg->gpsWnCorrection)<0) || ((navMsgCfg->gpsWnCorrection%1024)!=0))
	{
		/* use the default value */
		navMsgCfg->gpsWnCorrection = GPS_WEEK_NUMBER_CORRECTION;
	}

	/* get the correction for the Galileo WN
	 * Galileo WN starts from 0 at midnight between 21st and 22nd august 1999
	 * (when GPS goes from 1023 to 1024 (truncated to 0) */
	navMsgCfg->galileoWnCorrection = ((int)((navMsgCfg->gpsWnCorrection - 1024)/4096))*4096;

	/* Default Leap Seconds */
	strcpy(tag, "Default Leap Seconds");
	if(GetVal(cfgFilename,tag,(void *)&(navMsgCfg->defaultLeapSeconds),F_INTEGER,1) == FAILURE_RETURN)
	{
		/* use the default value */
		navMsgCfg->defaultLeapSeconds = DEFAULT_UTC_LEAP_SECOND;
	}
	else if(navMsgCfg->defaultLeapSeconds<0)
	{
		/* use the default value */
		navMsgCfg->defaultLeapSeconds = DEFAULT_UTC_LEAP_SECOND;
	}

	return SUCCESS_RETURN;
}

