/**
 * @file ncursesApi.c
 *
 * @author Mario Nicola
 *
 * @date 25/11/2014
 *
 * @version 1.0.0
 *
 * @brief Output to the NCurses interface
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ngeneConstants.h"
#include "ngeneDataStructure.h"
#include "ngeneConfiguration.h"
#include "ncursesApi.h"
#include "ioFunctions.h"
#include "closeManager.h"
#include "dop.h"
#include "timeConversions.h"
#include "timeConstants.h"


/* This global variable is used to print the name of folder containing the input files
 * when the receiver is executed in the NavMesg mode */
#if defined(FAKE_OSNMA_RECEIVER) && !defined(FAKE_CHIMERA_RECEIVER)
extern char navMesgFolderName[MAX_TAG_LEN];
#endif


void NcPrintString(TNCursesData *nc,int row,int col,const char *str)
{
	/* if str is not already the temporary string in TNCursesData structure, copy it!
	 * (maximum colsOld character) */
	if(str!=nc->tmpStr)
		strncpy(nc->tmpStr,str,nc->tmpStrLen);
	/* verify the goodness of column and row index */
	if((col>=COLS) || (row>=(LINES-1)))
	{
		return;
	}
	if(col<0)
	{
		col = 0;
	}
	/* if needed, limit the length of the string to be printed */
	if((col+(int)strlen(str)) > COLS)
	{
		nc->tmpStr[COLS-col-1] = '\0';
	}
	/* print! */
	mvwprintw(nc->uiWin,row,col,nc->tmpStr);
}


void NcCleanRow(TNCursesData *nc,int row)
{
	memset(nc->tmpStr,' ',COLS-1);
	nc->tmpStr[COLS-1] = '\0';
	NcPrintString(nc,row,0,nc->tmpStr);
}


void NcSetColorScheme(TNCursesData *nc,int scheme)
{
	if(nc->colorsEnabled==TRUE_FLAG)
	{
		attron(COLOR_PAIR(scheme));
	}
	refresh();
}


void NcUnsetColorScheme(TNCursesData *nc,int scheme)
{
	if(nc->colorsEnabled==TRUE_FLAG)
	{
		attroff(COLOR_PAIR(scheme));
	}
	refresh();
}


int InitNCurses(TNGeneConfiguration *cfg,TNGeneData *rx,int firstExecution)
{
	TNCursesData *nc = &(rx->ncursesData);
	char title[MAX_ROW_LENGTH];
	int lineIndex;

	/* Initialize the User Interface */
	nc->uiWin = initscr();
	cbreak(); /* raw(); is alternative to cbreak() */
	keypad(stdscr,TRUE);
	nodelay(nc->uiWin,TRUE);
	/* after initscr(), COLS and LINES stores the current number of columns and rows */
	if(firstExecution==TRUE_FLAG)
	{
		if(COLS>MAX_ROW_LENGTH)
		{
			nc->tmpStr = (char *)malloc((COLS+1)*sizeof(char));
			nc->tmpStrLen = COLS;
		}
		else
		{
			nc->tmpStr = (char *)malloc((MAX_ROW_LENGTH+1)*sizeof(char));
			nc->tmpStrLen = MAX_ROW_LENGTH;
		}
	}
	else if(COLS>nc->tmpStrLen)
	{
		free(nc->tmpStr);
		nc->tmpStr = (char *)malloc((COLS+1)*sizeof(char));
		nc->tmpStrLen = COLS;
	}
	if(nc->tmpStr==NULL)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR,"Malloc returned a NULL pointer in InitNCurses");
		CloseNGene(cfg,EXIT_FAILURE);
	}
	nc->colsOld = COLS;
	nc->linesOld = LINES;
	/* clean the terminal */
	memset(nc->tmpStr,' ',COLS-1);
	nc->tmpStr[COLS-1] = '\0';
	for(lineIndex=0;lineIndex<LINES;lineIndex++)
	{
		NcPrintString(nc,lineIndex,0,nc->tmpStr);
	}
	/* manage colors (if available) */
	if(has_colors()==FALSE)
	{
		nc->colorsEnabled = FALSE_FLAG;
	}
	else
	{
		nc->colorsEnabled = TRUE_FLAG;
		start_color();
		// Initialize color schemes
		init_pair(NC_COLOR_SCHEME_INFO, COLOR_WHITE, COLOR_BLACK);
		init_pair(NC_COLOR_SCHEME_NOTE, COLOR_YELLOW, COLOR_BLACK);
		init_pair(NC_COLOR_SCHEME_WARNING, COLOR_YELLOW, COLOR_BLACK);
		init_pair(NC_COLOR_SCHEME_ERROR, COLOR_RED, COLOR_BLACK);
		init_pair(NC_COLOR_SCHEME_TITLE, COLOR_BLUE, COLOR_YELLOW);
		init_pair(NC_COLOR_SCHEME_DATAOK, COLOR_GREEN, COLOR_BLACK);
		init_pair(NC_COLOR_SCHEME_ACQ, COLOR_YELLOW, COLOR_BLACK);
		init_pair(NC_COLOR_SCHEME_TRK, COLOR_CYAN, COLOR_BLACK);
		init_pair(NC_COLOR_SCHEME_NAV, COLOR_GREEN, COLOR_BLACK);
		init_pair(NC_COLOR_SCHEME_UNHEALTHY, COLOR_BLUE, COLOR_BLACK);
		init_pair(NC_COLOR_SCHEME_NO_OSNMA, COLOR_WHITE, COLOR_BLACK);
		init_pair(NC_COLOR_SCHEME_OSNMA_TEST, COLOR_YELLOW, COLOR_BLACK);
		init_pair(NC_COLOR_SCHEME_OSNMA_OP, COLOR_GREEN, COLOR_BLACK);
		init_pair(NC_COLOR_SCHEME_OSNMA_BAD, COLOR_RED, COLOR_BLACK);
	}
	/* Set row indexes for all the elements */
	nc->rowTitle = NC_ROW_TITLE;
	nc->rowDiamonds = NC_ROW_DIAMONDS;
	nc->rowTime = NC_ROW_TIME;
	nc->rowPvtTitle = NC_ROW_POSITION_TITLE;
	nc->rowPvtData = nc->rowPvtTitle + NC_NEXT_ROW;
	nc->rowChannelsTitle = NC_ROW_PRN_TABLE_TITLE;
	nc->rowFirstChannelData = nc->rowChannelsTitle + NC_NEXT_ROW + NC_EMPTY_ROW;
	nc->rowSettings = nc->rowFirstChannelData + cfg->numberOfChannels + NC_NEXT_ROW + NC_EMPTY_ROW;
	nc->rowStatus = nc->rowSettings + NC_NEXT_ROW;
	nc->rowMessages = nc->rowStatus + NC_NEXT_ROW;
	nc->rowCloseHint = nc->rowMessages + NC_NEXT_ROW + NC_EMPTY_ROW;
	/* Plot fixed elements on the User Interface */
	/* Hint to the user about the receiver close (always printed, whatever is the window size) */
	if(nc->rowCloseHint<(LINES-1))
	{
		strncpy(nc->tmpStr,"Press 'CTRL-c' to exit",nc->tmpStrLen);
		mvwprintw(nc->uiWin,nc->rowCloseHint,0,nc->tmpStr);
	}
	else
	{
		strncpy(nc->tmpStr,"Press 'CTRL-c' to exit (some logs could be cut due to the limited window size)",nc->tmpStrLen);
		mvwprintw(nc->uiWin,LINES-1,0,nc->tmpStr);
	}
	/* Print the Main Window Title */
	NcCleanRow(nc,nc->rowTitle);
	sprintf(title, "%s - v%s %s",NC_STR_TITLE,NGENE_VERSION_INDEX,NGENE_VERSION_NAME);
	NcSetColorScheme(nc,NC_COLOR_SCHEME_TITLE);
	NcPrintString(nc,nc->rowTitle,(COLS/2)-(strlen(title)/2) + 1,title);
	NcUnsetColorScheme(nc,NC_COLOR_SCHEME_TITLE);
	/* Print the position data title */
	NcPrintString(nc, nc->rowPvtTitle, NC_COL_LATITUDE, 	NC_STR_LATITUDE);
	NcPrintString(nc, nc->rowPvtTitle, NC_COL_LONGITUDE, 	NC_STR_LONGITUDE);
	NcPrintString(nc, nc->rowPvtTitle, NC_COL_ALTITUDE, 	NC_STR_ALTITUDE);
	NcPrintString(nc, nc->rowPvtTitle, NC_COL_VELOCITY, 	NC_STR_VELOCITY);
	NcPrintString(nc, nc->rowPvtTitle, NC_COL_GDOP, 		NC_STR_GDOP);
	/* Print the PRN table title */
	NcPrintString(nc, nc->rowChannelsTitle, NC_COL_TABLE_PRN, 	  NC_STR_TABLE_PRN);
	NcPrintString(nc, nc->rowChannelsTitle, NC_COL_TABLE_STATE,   NC_STR_TABLE_STATE);
	NcPrintString(nc, nc->rowChannelsTitle, NC_COL_TABLE_CN0, 	  NC_STR_TABLE_CN0);
	NcPrintString(nc, nc->rowChannelsTitle, NC_COL_TABLE_DOPPLER, NC_STR_TABLE_DOPPLER);
	NcPrintString(nc, nc->rowChannelsTitle, NC_COL_TABLE_OSNMA,   NC_STR_TABLE_OSNMA);
	NcPrintString(nc, nc->rowChannelsTitle, NC_COL_TABLE_DSM,     NC_STR_TABLE_DSM);
	NcPrintString(nc, nc->rowChannelsTitle, NC_COL_TABLE_KEY,     NC_STR_TABLE_KEY);
	NcPrintString(nc, nc->rowChannelsTitle, NC_COL_TABLE_NAVMSG,  NC_STR_TABLE_NAVMSG);

	/* Receiver status */
	NcPrintString(nc, nc->rowStatus, 0,				NC_STR_STATUS);
	NcPrintString(nc, nc->rowStatus, NC_COL_STATUS, NC_STR_STATUS_RX_RUNNING);
	/* Status of the receiver */
	sprintf(nc->tmpStr,"NGene2 receiver elaboration started");
	NcDrawRxLog(nc,nc->tmpStr,NGENE_LOG_SEVERITY_NOTE);
	/* prints configuration of the input */
	if(cfg->inputMethod==USB_FRONTEND)
	{
		sprintf(nc->tmpStr,"Running front end mode from <%s> USB front end",cfg->fePars->name);
	}
	else /* if(cfg->inputMethod==INPUT_FILE) */
	{
		#ifndef FAKE_OSNMA_RECEIVER
		sprintf(nc->tmpStr,"Running off-line mode from file <%s> (%s front end)",cfg->sampleFilename,cfg->fePars->name);
		#else
		sprintf(nc->tmpStr,"NGene2 NavMesg mode, inputs from %s",navMesgFolderName);
		#endif
	}
	NcPrintString(nc,nc->rowSettings, 0, nc->tmpStr);
	refresh();
	/**/
	cfg->ncursesData = nc;
	return SUCCESS_RETURN;
}


void DeinitNCurses(TNCursesData *nc)
{
	free(nc->tmpStr);
	endwin();
}


int NcDrawRxLog(TNCursesData *nc,char *logString, int severity)
{
	if(nc==NULL)
	{
		return FAILURE_RETURN;
	}

	NcCleanRow(nc,nc->rowMessages);
	switch(severity)
	{
		case NGENE_LOG_SEVERITY_NOTE:
			NcSetColorScheme(nc,NC_COLOR_SCHEME_NOTE);
			NcPrintString(nc,nc->rowMessages,NC_COL_LOG,logString);
			refresh();
			NcUnsetColorScheme(nc,NC_COLOR_SCHEME_NOTE);
			break;
		case NGENE_LOG_SEVERITY_INFO:
		case NGENE_LOG_SEVERITY_IMPORTANT_INFO:
			NcSetColorScheme(nc,NC_COLOR_SCHEME_NOTE);
			NcPrintString(nc,nc->rowMessages,NC_COL_LOG,logString);
			refresh();
			NcUnsetColorScheme(nc,NC_COLOR_SCHEME_NOTE);
			break;
		case NGENE_LOG_SEVERITY_WARNING:
			NcSetColorScheme(nc,NC_COLOR_SCHEME_WARNING);
			NcPrintString(nc,nc->rowMessages,NC_COL_LOG,logString);
			refresh();
			NcUnsetColorScheme(nc,NC_COLOR_SCHEME_WARNING);
			break;
		case NGENE_LOG_SEVERITY_ERROR:
			NcSetColorScheme(nc,NC_COLOR_SCHEME_ERROR);
			NcPrintString(nc,nc->rowMessages,NC_COL_LOG,logString);
			refresh();
			NcUnsetColorScheme(nc,NC_COLOR_SCHEME_ERROR);
			break;
		default:
			break;
	}
	return SUCCESS_RETURN;
}


void NcCompleteLog(TNGeneConfiguration *cfg,TNGeneData *rx)
{
	TNCursesData *nc = &(rx->ncursesData);
	TChannel *ch;
	TPvtResults *pvtRes = &(rx->pvtResults);
	TPvtData *pvtData = &(rx->pvtData);
	double velocity = sqrt(pow(pvtRes->ecefVelocity[0],2.0)+pow(pvtRes->ecefVelocity[1],2.0)+pow(pvtRes->ecefVelocity[2],2.0))*SECONDS_IN_A_HOUR/1000;
	int plotData;
	int chIdx;
	int prnAcq = 0;
	int prnTrk = 0;
	int prnNav = 0;
	int prnIdle = 0;
	int wn=0;
	int tow=0;
	char curTimeType[4];
	int curTimeValid;
	TDateTime curTime;
	const char monthsNames[12][4] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
	int chRow;
	int curColorScheme;

	if((nc->colsOld!=COLS) || (nc->linesOld!=LINES))
	{
		InitNCurses(cfg,rx,FALSE_FLAG);
	}
	if(pvtRes->pvtValid==TRUE_FLAG)
	{
		plotData = TRUE_FLAG;
	}
	else
	{
		plotData = FALSE_FLAG;
	}
	NcCleanRow(nc,nc->rowPvtData);
	NcCleanRow(nc,nc->rowTime);
	NcCleanRow(nc,nc->rowDiamonds);
	// Plot the number of channels
	for(chIdx=0;(chIdx<cfg->numberOfChannels);chIdx++)
	{
		ch = &(rx->channelArray[chIdx]);
		switch(ch->chState)
		{
			case COARSE_ACQUISITION_STATE:
			case CODE_REFINEMENT_STATE:
			case DOPPLER_REFINEMENT_STATE:
			case CONFIRMATION_STATE:
				prnAcq++;
				break;
			case TRACKING_STATE:
				if(ch->pvtInfo.usedForPVT==TRUE_FLAG)
				{
					prnNav++;
				}
				else
				{
					prnTrk++;
				}
				break;
			case POWERON_STATE:
			case UNUSED_STATE:
			default:
				prnIdle++;
				break;
		}
	}

	/* If the position is valid display, diamonds of different colors indicating satellites in tracking or navigation */
	/* used in PVT */
	NcSetColorScheme(nc,NC_COLOR_SCHEME_NAV);
	for(chIdx=0;chIdx<(prnNav);chIdx++)
	{
		mvaddch(nc->rowDiamonds,NC_COL_DIAMONDS+chIdx,ACS_DIAMOND);
	}
	NcUnsetColorScheme(nc,NC_COLOR_SCHEME_NAV);
	/* tracked but not used in PVT */
	NcSetColorScheme(nc,NC_COLOR_SCHEME_TRK);
	for(chIdx=prnNav;chIdx<(prnNav+prnTrk);chIdx++)
	{
		mvaddch(nc->rowDiamonds,NC_COL_DIAMONDS+chIdx,ACS_DIAMOND);
	}
	NcUnsetColorScheme(nc,NC_COLOR_SCHEME_TRK);
	/* acquisition */
	NcSetColorScheme(nc,NC_COLOR_SCHEME_ACQ);
	for(chIdx=(prnNav+prnTrk);chIdx<(prnNav+prnTrk+prnAcq);chIdx++)
	{
		mvaddch(nc->rowDiamonds,NC_COL_DIAMONDS+chIdx,ACS_DIAMOND);
	}
	NcUnsetColorScheme(nc,NC_COLOR_SCHEME_ACQ);
	/* idle */
	for(chIdx=(prnNav+prnTrk+prnAcq);chIdx<(prnNav+prnTrk+prnAcq+prnIdle);chIdx++)
	{
		mvaddch(nc->rowDiamonds,NC_COL_DIAMONDS+chIdx,ACS_DIAMOND);
	}
	/* Plot position (if available)
	 * If a new position is not available, plot the old one but with a different color */
	if((plotData==TRUE_FLAG) && (pvtRes->pvtPlotted==FALSE_FLAG))
	{
		NcSetColorScheme(nc,NC_COLOR_SCHEME_DATAOK);
	}
	/* Latitude */
	if((plotData==TRUE_FLAG) || (pvtRes->pvtPlotted==TRUE_FLAG))
	{
		sprintf(nc->tmpStr,"%10.5f",pvtRes->geodPosition[1]);
	}
	else
	{
		strncpy(nc->tmpStr,NC_STR_UNKNOWN,nc->tmpStrLen);
	}
	NcPrintString(nc,nc->rowPvtData, NC_COL_LATITUDE,nc->tmpStr);
	if((plotData==TRUE_FLAG) || (pvtRes->pvtPlotted==TRUE_FLAG))
	{
		mvaddch(nc->rowPvtData, NC_COL_LATITUDE+10, ACS_DEGREE);
	}
	/* Longitude */
	if((plotData==TRUE_FLAG) || (pvtRes->pvtPlotted==TRUE_FLAG))
	{
		sprintf(nc->tmpStr,"%10.5f",pvtRes->geodPosition[0]);
	}
	else
	{
		strncpy(nc->tmpStr,NC_STR_UNKNOWN,nc->tmpStrLen);
	}
	NcPrintString(nc,nc->rowPvtData, NC_COL_LONGITUDE,nc->tmpStr);
	if((plotData==TRUE_FLAG) || (pvtRes->pvtPlotted==TRUE_FLAG))
	{
		mvaddch(nc->rowPvtData,NC_COL_LONGITUDE+10,ACS_DEGREE);
	}
	/* Altitude */
	if((plotData==TRUE_FLAG) || (pvtRes->pvtPlotted==TRUE_FLAG))
	{
		sprintf(nc->tmpStr,"%.1lf",pvtRes->geodPosition[2]);
	}
	else
	{
		strncpy(nc->tmpStr,NC_STR_UNKNOWN,nc->tmpStrLen);
	}
	NcPrintString(nc,nc->rowPvtData,NC_COL_ALTITUDE,nc->tmpStr);
	/* Velocity */
	if((plotData==TRUE_FLAG) || (pvtRes->pvtPlotted==TRUE_FLAG))
	{
		sprintf(nc->tmpStr,"%.1lf",velocity);
	}
	else
	{
		strncpy(nc->tmpStr,NC_STR_UNKNOWN,nc->tmpStrLen);
	}
	NcPrintString(nc,nc->rowPvtData,NC_COL_VELOCITY,nc->tmpStr);
	/* Plot date and time */
	curTimeValid=FALSE_FLAG;
	/* if the time out of the PVT is valid and updated, the time out of the PVT is displayed
	 * else plot the predicted time (if available), else plot "--" */
	if(pvtRes->rxUtcTime.referenceUpdated==TRUE_FLAG)
	{
		if(pvtRes->utcFromGps==TRUE_FLAG)
		{
			wn = pvtRes->rxUtcTime.referenceWn;
			tow = (int)floor(pvtRes->rxUtcTime.referenceTow);
			FromGnssTimeToDate(GPS_SAT,wn,tow,&curTime);
			sprintf(curTimeType,"UTC");
			curTimeValid=TRUE_FLAG;
		}
		if(pvtRes->utcFromGal==TRUE_FLAG)
		{
			wn = pvtRes->rxUtcTime.referenceWn;
			tow = (int)floor(pvtRes->rxUtcTime.referenceTow);
			FromGnssTimeToDate(GALILEO_SAT,wn,tow,&curTime);
			sprintf(curTimeType,"UTC");
			curTimeValid=TRUE_FLAG;
		}
	}
	else if((pvtRes->utcFromGps==TRUE_FLAG) && (pvtData->rxUtcTimeValid==TRUE_FLAG))
	{
		wn = pvtData->rxUtcWn;
		tow = (int)floor(pvtData->rxUtcTow);
		FromGnssTimeToDate(GPS_SAT,wn,tow,&curTime);
		sprintf(curTimeType,"UTC");
		curTimeValid=TRUE_FLAG;
	}
	else if((pvtRes->utcFromGal==TRUE_FLAG) && (pvtData->rxUtcTimeValid==TRUE_FLAG))
	{
		wn = pvtData->rxUtcWn;
		tow = (int)floor(pvtData->rxUtcTow);
		FromGnssTimeToDate(GALILEO_SAT,wn,tow,&curTime);
		sprintf(curTimeType,"UTC");
		curTimeValid=TRUE_FLAG;
	}
	else if(pvtRes->rxGpsTime.referenceUpdated==TRUE_FLAG)
	{
		wn = pvtRes->rxGpsTime.referenceWn;
		tow = (int)floor(pvtRes->rxGpsTime.referenceTow);
		FromGnssTimeToDate(GPS_SAT,wn,tow,&curTime);
		sprintf(curTimeType,"GPS");
		curTimeValid=TRUE_FLAG;
	}
	else if(pvtData->rxGpsTimeValid==TRUE_FLAG)
	{
		wn = pvtData->rxGpsWn;
		tow = (int)floor(pvtData->rxGpsTow);
		FromGnssTimeToDate(GPS_SAT,wn,tow,&curTime);
		sprintf(curTimeType,"GPS");
		curTimeValid=TRUE_FLAG;
	}
	else if(pvtRes->rxGalTime.referenceUpdated==TRUE_FLAG)
	{
		wn = pvtRes->rxGalTime.referenceWn;
		tow = (int)floor(pvtRes->rxGalTime.referenceTow);
		FromGnssTimeToDate(GALILEO_SAT,wn,tow,&curTime);
		sprintf(curTimeType,"Gal");
		curTimeValid=TRUE_FLAG;
	}
	else if(pvtData->rxGalTimeValid==TRUE_FLAG)
	{
		wn = pvtData->rxGalWn;
		tow = (int)floor(pvtData->rxGalTow);
		FromGnssTimeToDate(GALILEO_SAT,wn,tow,&curTime);
		sprintf(curTimeType,"Gal");
		curTimeValid=TRUE_FLAG;
	}
	if((curTimeValid==TRUE_FLAG) && (pvtRes->utcFromGal==TRUE_FLAG))
	{
		sprintf(nc->tmpStr, "%s %2d, %4d \t %2d:%02d:%02d (%s)\t WN Gal: %d \t TOW: %d",
				monthsNames[curTime.month-1],curTime.day,curTime.year,curTime.hour,curTime.minute,(int)curTime.second,curTimeType,wn,tow);
	}
	else if((curTimeValid==TRUE_FLAG) && (pvtRes->utcFromGps==TRUE_FLAG))
	{
		sprintf(nc->tmpStr, "%s %2d, %4d \t %2d:%02d:%02d (%s)\t WN GPS: %d \t TOW: %d",
				monthsNames[curTime.month-1],curTime.day,curTime.year,curTime.hour,curTime.minute,(int)curTime.second,curTimeType,wn,tow);
	}
	else
	{
		strncpy(nc->tmpStr,NC_STR_UNKNOWN,nc->tmpStrLen);
	}
	NcPrintString(nc,nc->rowTime,NC_COL_TIME,nc->tmpStr);
	/* Plot GDOP */
	if(plotData==TRUE_FLAG)
	{
		sprintf(nc->tmpStr,"%.2lf",pvtRes->dop[GDOP_INDEX_IN_DOP_ARRAY]);
	}
	else
	{
		strncpy(nc->tmpStr,NC_STR_UNKNOWN,nc->tmpStrLen);
	}
	NcPrintString(nc,nc->rowPvtData,NC_COL_GDOP,nc->tmpStr);

	if((plotData==TRUE_FLAG) && (pvtRes->pvtPlotted==FALSE_FLAG))
	{
		NcUnsetColorScheme(nc,NC_COLOR_SCHEME_DATAOK);
		pvtRes->pvtPlotted = TRUE_FLAG;
	}
	/* Plot all the information about all the channels */
	for(chIdx=0;chIdx<cfg->numberOfChannels;chIdx++)
	{
		ch = &(rx->channelArray[chIdx]);
		/* clean the space for the channel */
		chRow = nc->rowFirstChannelData + chIdx;
		NcCleanRow(nc,chRow);
		curColorScheme = -1;
		/* channel state */
		switch(ch->chState)
		{
			case POWERON_STATE:
			case UNUSED_STATE:
				strncpy(nc->tmpStr,NC_STR_CH_POWERON_STATE,nc->tmpStrLen);
				NcPrintString(nc,chRow,NC_COL_TABLE_STATE,nc->tmpStr);
				break;
			case COARSE_ACQUISITION_STATE:
			case CODE_REFINEMENT_STATE:
			case DOPPLER_REFINEMENT_STATE:
			case CONFIRMATION_STATE:
				strncpy(nc->tmpStr,NC_STR_CH_ACQUISITION_STATE,nc->tmpStrLen);
				NcSetColorScheme(nc,NC_COLOR_SCHEME_ACQ);
				NcPrintString(nc,chRow,NC_COL_TABLE_STATE,nc->tmpStr);
				curColorScheme = NC_COLOR_SCHEME_ACQ;
				break;
			case TRACKING_STATE:
				if((ch->pvtInfo.usedForPVT==TRUE_FLAG) && (rx->osnmaAuth.authenticationFlags[chIdx]==VALIDATED))
				{
					strncpy(nc->tmpStr,NC_STR_CH_AUTHENTICATED_STATE,nc->tmpStrLen);
					NcSetColorScheme(nc,NC_COLOR_SCHEME_NAV);
					NcPrintString(nc,chRow,NC_COL_TABLE_STATE,nc->tmpStr);
					curColorScheme = NC_COLOR_SCHEME_NAV;
				}
				else if(ch->pvtInfo.usedForPVT==TRUE_FLAG)
				{
					strncpy(nc->tmpStr,NC_STR_CH_NAVIGATION_STATE,nc->tmpStrLen);
					NcSetColorScheme(nc,NC_COLOR_SCHEME_NAV);
					NcPrintString(nc,chRow,NC_COL_TABLE_STATE,nc->tmpStr);
					curColorScheme = NC_COLOR_SCHEME_NAV;
				}
				else
				{
					NcSetColorScheme(nc,NC_COLOR_SCHEME_TRK);
					curColorScheme = NC_COLOR_SCHEME_TRK;
					if(ch->satType==GPS_SAT)
					{
						if(rx->osnmaAuth.authenticationFlags[chIdx]==NOT_VALID)
						{
							strncpy(nc->tmpStr,NC_STR_CH_NOT_AUTH_STATE,nc->tmpStrLen);
							NcUnsetColorScheme(nc,NC_COLOR_SCHEME_TRK);
							NcSetColorScheme(nc,NC_COLOR_SCHEME_ERROR);
							NcPrintString(nc,chRow,NC_COL_TABLE_STATE,nc->tmpStr);
							NcUnsetColorScheme(nc,NC_COLOR_SCHEME_ERROR);
							NcSetColorScheme(nc,NC_COLOR_SCHEME_TRK);
						}
						else if(ch->gpsParser->navMsg->eph.health!=SIGNAL_OK)
						{
							strncpy(nc->tmpStr,NC_STR_CH_UNHEALTHY_STATE,nc->tmpStrLen);
							NcPrintString(nc,chRow,NC_COL_TABLE_STATE,nc->tmpStr);
						}
						else
						{
							strncpy(nc->tmpStr,NC_STR_CH_TRACKING_STATE,nc->tmpStrLen);
							NcPrintString(nc,chRow,NC_COL_TABLE_STATE,nc->tmpStr);
						}
					}
					if((ch->satType==GALILEO_SAT) && (ch->satSignal==E1_BC))
					{
						if(rx->osnmaAuth.authenticationFlags[chIdx]==NOT_VALID)
						{
							strncpy(nc->tmpStr,NC_STR_CH_NOT_AUTH_STATE,nc->tmpStrLen);
							NcUnsetColorScheme(nc,NC_COLOR_SCHEME_TRK);
							NcSetColorScheme(nc,NC_COLOR_SCHEME_ERROR);
							NcPrintString(nc,chRow,NC_COL_TABLE_STATE,nc->tmpStr);
							NcUnsetColorScheme(nc,NC_COLOR_SCHEME_ERROR);
							NcSetColorScheme(nc,NC_COLOR_SCHEME_TRK);
						}
						else if(ch->galParser->navMsg->eph.health!=SIGNAL_OK)
						{
							strncpy(nc->tmpStr,NC_STR_CH_UNHEALTHY_STATE,nc->tmpStrLen);
							NcPrintString(nc,chRow,NC_COL_TABLE_STATE,nc->tmpStr);
						}
						else if(ch->galParser->navMsg->eph.galileoDataValidity==WORKING_WITHOUT_GUARANTEE)
						{
							strncpy(nc->tmpStr,NC_STR_CH_NAVMSG_INVALID_STATE,nc->tmpStrLen);
							NcPrintString(nc,chRow,NC_COL_TABLE_STATE,nc->tmpStr);
						}
						else
						{
							strncpy(nc->tmpStr,NC_STR_CH_TRACKING_STATE,nc->tmpStrLen);
							NcPrintString(nc,chRow,NC_COL_TABLE_STATE,nc->tmpStr);
						}
					}
					if((ch->satType==GALILEO_SAT) && (ch->satSignal==E5_A))
					{
						if(rx->osnmaAuth.authenticationFlags[chIdx]==NOT_VALID)
						{
							strncpy(nc->tmpStr,NC_STR_CH_NOT_AUTH_STATE,nc->tmpStrLen);
							NcUnsetColorScheme(nc,NC_COLOR_SCHEME_TRK);
							NcSetColorScheme(nc,NC_COLOR_SCHEME_ERROR);
							NcPrintString(nc,chRow,NC_COL_TABLE_STATE,nc->tmpStr);
							NcUnsetColorScheme(nc,NC_COLOR_SCHEME_ERROR);
							NcSetColorScheme(nc,NC_COLOR_SCHEME_TRK);
						}
						else if(ch->fnavParser->navMsg->eph.health!=SIGNAL_OK)
						{
							strncpy(nc->tmpStr,NC_STR_CH_UNHEALTHY_STATE,nc->tmpStrLen);
							NcPrintString(nc,chRow,NC_COL_TABLE_STATE,nc->tmpStr);
						}
						else if(ch->fnavParser->navMsg->eph.galileoDataValidity==WORKING_WITHOUT_GUARANTEE)
						{
							strncpy(nc->tmpStr,NC_STR_CH_NAVMSG_INVALID_STATE,nc->tmpStrLen);
							NcPrintString(nc,chRow,NC_COL_TABLE_STATE,nc->tmpStr);
						}
						else
						{
							strncpy(nc->tmpStr,NC_STR_CH_TRACKING_STATE,nc->tmpStrLen);
							NcPrintString(nc,chRow,NC_COL_TABLE_STATE,nc->tmpStr);
						}
					}
					if(ch->satType==EGNOS_SAT)
					{
						strncpy(nc->tmpStr,NC_STR_CH_TRACKING_STATE,nc->tmpStrLen);
						NcPrintString(nc,chRow,NC_COL_TABLE_STATE,nc->tmpStr);
					}
				}
				break;
			default:
				break;
		}
		/* channel index */
		sprintf(nc->tmpStr,"%2d",chIdx);
		NcPrintString(nc,chRow,NC_COL_TABLE_CHANNEL,nc->tmpStr);
		/* PRN */
		strncpy(nc->tmpStr,NC_STR_UNKNOWN,nc->tmpStrLen);
		if(ch->satId != INVALID_PRN)
		{
			switch(ch->satType)
			{
				case GPS_SAT:
					sprintf(nc->tmpStr,"%s %3d",NC_STR_PRN_IS_GPS,ch->satId);
					break;
				case GALILEO_SAT:
					if(ch->satSignal==E1_BC)
						sprintf(nc->tmpStr,"%s %3d",NC_STR_PRN_IS_GALILEO,ch->satId);
					else
						sprintf(nc->tmpStr,"%s %3d",NC_STR_PRN_IS_E5A,ch->satId);
					break;
				default: // EGNOS_SAT
					sprintf(nc->tmpStr,"%s %3d",NC_STR_PRN_IS_EGNOS,ch->satId);
					break;
			}
		}
		NcPrintString(nc,chRow,NC_COL_TABLE_PRN,nc->tmpStr);
		/* CN0 */
		if(ch->chState==TRACKING_STATE)
		{
			sprintf(nc->tmpStr,"%4.1f",ch->tracking.trkStatusChecker.cnoEstimator.estimatedCNo);
		}
		else
		{
			strncpy(nc->tmpStr,NC_STR_EMPTY,nc->tmpStrLen);
		}
		NcPrintString(nc,chRow,NC_COL_TABLE_CN0,nc->tmpStr);
		/* Doppler */
		if(ch->chState==TRACKING_STATE)
		{
			sprintf(nc->tmpStr,"%7.1lf",ch->dopplerShift);
		}
		else
		{
			strncpy(nc->tmpStr,NC_STR_EMPTY,nc->tmpStrLen);
		}
		NcPrintString(nc,chRow,NC_COL_TABLE_DOPPLER,nc->tmpStr);
		/* Reset the state info color */
		if(curColorScheme>0)
		{
			/* the color code is valid */
			NcUnsetColorScheme(nc,curColorScheme);
		}
		/* OSNMA status */
		if((ch->satType==GALILEO_SAT) && (ch->satSignal==E1_BC) && (ch->chState==TRACKING_STATE))
		{
			NcSetColorScheme(nc,ch->galParser->chOsnma.chOsnmaStatusLogColor);
			NcPrintString(nc,chRow,NC_COL_TABLE_OSNMA,ch->galParser->chOsnma.chOsnmaStatusLog);
			NcUnsetColorScheme(nc,ch->galParser->chOsnma.chOsnmaStatusLogColor);
		}
		/* OSNMA validation results */
		/* Digital Signature */
		if(ch->dsmStatus==VALIDATED)
		{
			NcSetColorScheme(nc,NC_COLOR_SCHEME_DATAOK);
			NcPrintString(nc,chRow,NC_COL_TABLE_DSM,"Validated");
			NcUnsetColorScheme(nc,NC_COLOR_SCHEME_DATAOK);
		}
		else if(ch->dsmStatus==NOT_VALID)
		{
			NcSetColorScheme(nc,NC_COLOR_SCHEME_ERROR);
			NcPrintString(nc,chRow,NC_COL_TABLE_DSM,"Not Valid");
			NcUnsetColorScheme(nc,NC_COLOR_SCHEME_ERROR);
		}
		else
		{
			NcPrintString(nc,chRow,NC_COL_TABLE_DSM,NC_STR_UNKNOWN);
		}

		/* TESLA chain key */
		if(ch->keyStatus==VALIDATED)
		{
			NcSetColorScheme(nc,NC_COLOR_SCHEME_DATAOK);
			NcPrintString(nc,chRow,NC_COL_TABLE_KEY,"Validated");
			NcUnsetColorScheme(nc,NC_COLOR_SCHEME_DATAOK);
		}
		else if(ch->keyStatus==NOT_VALID)
		{
			NcSetColorScheme(nc,NC_COLOR_SCHEME_ERROR);
			NcPrintString(nc,chRow,NC_COL_TABLE_KEY,"Not Valid");
			NcUnsetColorScheme(nc,NC_COLOR_SCHEME_ERROR);
		}
		else
		{
			NcPrintString(nc,chRow,NC_COL_TABLE_KEY,NC_STR_UNKNOWN);
		}
		/* Navigation message */
		if(ch->msgStatus==VALIDATED)
		{
			NcSetColorScheme(nc,NC_COLOR_SCHEME_DATAOK);
			NcPrintString(nc,chRow,NC_COL_TABLE_NAVMSG,"Validated");
			NcUnsetColorScheme(nc,NC_COLOR_SCHEME_DATAOK);
		}
		else if(ch->msgStatus==NOT_VALID)
		{
			NcSetColorScheme(nc,NC_COLOR_SCHEME_ERROR);
			NcPrintString(nc,chRow,NC_COL_TABLE_NAVMSG,"Not Valid");
			NcUnsetColorScheme(nc,NC_COLOR_SCHEME_ERROR);
		}
		else
		{
			NcPrintString(nc,chRow,NC_COL_TABLE_NAVMSG,NC_STR_UNKNOWN);
		}
	}
	refresh();
}
