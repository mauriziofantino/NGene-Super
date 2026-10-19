/**
 * @file timeConversions.c
 * @author Mario Nicola
 * @date 30/10/2014
 *
 * @brief Implementation of the time conversions utility
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include "ngeneDataStructure.h"
#include "timeConversions.h"
#include "timeConstants.h"
#include "spreadingCodes.h"
#include "ngeneConstants.h"
#include "ioFunctions.h"


#define IS_LEAP(year) ((((year % 4) == 0) && ((year % 100) != 0)) || ((year % 400) == 0))
#define IS_LONG(month) ((month == JANUARY) || (month == MARCH) || (month == MAY) || (month == JULY) || (month == AUGUST) || (month == OCTOBER) || (month == DECEMBER))


void FromGnssTimeToDate(TSatConstellation system, int wn, double tow, TDateTime *datePtr)
{
	/* TODO Da ottimizzare? */
	double ss;
	unsigned int gg;
	double ssInDay, ssInHour;
	unsigned int ggCnt = 0;
	unsigned int newGgCnt = 0;
	unsigned int year;
	unsigned int ggInYear;
	TMonth month;
	unsigned int firstDay;
	int galileoCorrection;

	/* start epoch changes in function of constellation (GPS/Galileo) */
	if(system==GPS_SAT)
	{
		year = FIRST_GPS_YEAR;
		month = FIRST_GPS_MONTH;
		firstDay = FIRST_GPS_DAY;
		galileoCorrection = 0;
	}
	else
	{
		year = FIRST_GALILEO_YEAR;
		month = FIRST_GALILEO_MONTH;
		firstDay = FIRST_GALILEO_DAY;
		galileoCorrection = 1;
	}

	/* number of seconds from the start epoch */
	ss = (double)(wn * SECONDS_IN_A_WEEK) + tow;
	gg = (unsigned int)floor(ss/(double)SECONDS_IN_A_DAY) + firstDay;
	ssInDay = fmod(ss,(double)SECONDS_IN_A_DAY);
	ssInHour = fmod(ssInDay,(double)SECONDS_IN_A_HOUR);

	/* set year */
	while(newGgCnt<gg)
	{
		ggCnt = newGgCnt;
		if(IS_LEAP((year+galileoCorrection)))
		{
			newGgCnt += DAYS_IN_A_LEAP_YEAR;
		}
		else
		{
			newGgCnt += DAYS_IN_A_YEAR;
		}
		year += 1;
	}
	year -=1;
	datePtr->year = year;

	/* set month */
	ggInYear = gg - ggCnt;
	newGgCnt = 0;
	ggCnt = 0;
	while(newGgCnt < ggInYear)
	{
		ggCnt = newGgCnt;
		if(month == FEBRUARY)
		{
			if(IS_LEAP(year))
			{
				newGgCnt = newGgCnt + DAYS_IN_FEBRUARY_LEAP_YEAR;
			}
			else
			{
				newGgCnt = newGgCnt + DAYS_IN_FEBRUARY_NO_LEAP_YEAR;
			}
		}
		else if(IS_LONG(month))
		{
			newGgCnt = newGgCnt + DAYS_IN_A_LONG_MONTH;
		}
		else
		{
			newGgCnt = newGgCnt + DAYS_IN_A_SHORT_MONTH;
		}
		year = (month==DECEMBER) ? year+1 : year;
		month = (month==DECEMBER) ? JANUARY : month + 1;
	}
	datePtr->year = (month==JANUARY) ? year-1 : year;
	datePtr->month = (month==JANUARY) ? DECEMBER : (month -1);

	/* set day, hour, minute and second */
	datePtr->day = (ggInYear - ggCnt);
	datePtr->hour = floor(ssInDay/(double)SECONDS_IN_A_HOUR);
	datePtr->minute = floor(ssInHour/(double)SECONDS_IN_A_MINUTE);
	datePtr->second = fmod(ssInHour,(double)SECONDS_IN_A_MINUTE);
}


void FromDateToGnssTime(TSatConstellation system, int *wn, double *tow, TDateTime *datePtr)
{
	/* TODO da ottimizzare? */
	unsigned int day;
	TMonth month;
	unsigned int year;

	/* start epoch changes in function of constellation (GPS/Galileo) */
	if(system==GPS_SAT)
	{
		year = FIRST_GPS_YEAR;
		month = FIRST_GPS_MONTH;
		day = FIRST_GPS_DAY;
	}
	else
	{
		year = FIRST_GALILEO_YEAR;
		month = FIRST_GALILEO_MONTH;
		day = FIRST_GALILEO_DAY;
	}
	/* initialize */
	*tow = 0.0;
	/* compute total number of seconds from start epoch */
	while(((int)year<datePtr->year) || (month<datePtr->month) || ((int)day<datePtr->day))
	{
		*tow += (double)SECONDS_IN_A_DAY;
		day++;
		if((IS_LEAP(year)) && (month==FEBRUARY) && (day==(DAYS_IN_FEBRUARY_LEAP_YEAR+1)))
		{
			month=MARCH;
			day=1;
		}
		else if((!(IS_LEAP(year))) && (month==FEBRUARY) && (day==(DAYS_IN_FEBRUARY_NO_LEAP_YEAR+1)))
		{
			month=MARCH;
			day=1;
		}
		else if((month==DECEMBER) && (day==(DAYS_IN_A_LONG_MONTH+1)))
		{
			year++;
			month=JANUARY;
			day=1;
		}
		else if((IS_LONG(month)) && (day==(DAYS_IN_A_LONG_MONTH+1)))
		{
			month++;
			day=1;
		}
		else if((!IS_LONG(month)) && day==(DAYS_IN_A_SHORT_MONTH+1))
		{
			month++;
			day=1;
		}
	}
	/* adjust WN and TOW */
	*tow += ((datePtr->hour*(double)SECONDS_IN_A_HOUR)+(datePtr->minute*(double)SECONDS_IN_A_MINUTE)+datePtr->second);
	*wn = (int)floor(*tow / (double)SECONDS_IN_A_WEEK);
	*tow = fmod(*tow,(double)SECONDS_IN_A_WEEK);
}


int FromDayNumberToDate(int dayOfTheYear, TDateTime *datePtr)
{
	int monthLength;

	if((IS_LEAP(datePtr->year)) && (dayOfTheYear>DAYS_IN_A_LEAP_YEAR))
	{
		return 1;
	}
	else if((!(IS_LEAP(datePtr->year))) && (dayOfTheYear>DAYS_IN_A_YEAR))
	{
		return 1;
	}
	/**/
	datePtr->month = JANUARY;
	datePtr->day = 1;
	monthLength = DAYS_IN_A_LONG_MONTH;
	while(dayOfTheYear>monthLength)
	{
		dayOfTheYear -= monthLength;
		// next month
		datePtr->month++;
		if(IS_LONG(datePtr->month))
		{
			monthLength = DAYS_IN_A_LONG_MONTH;
		}
		else if((datePtr->month==FEBRUARY) && (IS_LEAP(datePtr->year)))
		{
			monthLength = DAYS_IN_FEBRUARY_LEAP_YEAR;
		}
		else if(datePtr->month==FEBRUARY) /* February, no leap year */
		{
			monthLength = DAYS_IN_FEBRUARY_NO_LEAP_YEAR;
		}
		else /* short month, not February */
		{
			monthLength = DAYS_IN_A_SHORT_MONTH;
		}
	}
	datePtr->day = dayOfTheYear;
	return 0;
}


int FromDateToDayNumber(TDateTime *datePtr)
{
	int dayOfTheYear = 0;
	TMonth month = JANUARY;
	int monthLength;

	/* add month duration */
	while(month<datePtr->month)
	{
		if(IS_LONG(month))
		{
			monthLength = DAYS_IN_A_LONG_MONTH;
		}
		else if((month==FEBRUARY) && (IS_LEAP(datePtr->year)))
		{
			monthLength = DAYS_IN_FEBRUARY_LEAP_YEAR;
		}
		else if(month==FEBRUARY) /* February, no leap year */
		{
			monthLength = DAYS_IN_FEBRUARY_NO_LEAP_YEAR;
		}
		else /* short month, not February */
		{
			monthLength = DAYS_IN_A_SHORT_MONTH;
		}
		dayOfTheYear += monthLength;
		month++;
	}
	/* current month*/
	dayOfTheYear += datePtr->day;
	return dayOfTheYear;
}


int GetGstToGpsTimeOffset(TNGeneData *rx,int gstWn, double gstTow, double *gstToGpsTimeOffset)
{
	int idx;
	TGalileoToGpsTimeConversion *galToGps = NULL;
	int deltaWn;
	int wn0G;

	/* select the first valid set of GST to GPS time conversion parameters */
	idx = 0;
	while((idx<GALILEO_NUMBER_OF_PRNS) && (rx->galNavigationMessage[idx].gal2GpsReceived==FALSE_FLAG))
	{
		idx++;
	}
	if(idx<GALILEO_NUMBER_OF_PRNS)
	{
		/* is a set of valid parameters has been found, calculate the offset */
		galToGps = &(rx->galNavigationMessage[idx].gal2Gps);
		// get the delta on the week number.
		if(rx->galNavigationMessage[idx].gal2GpsCompleted==TRUE_FLAG)
		{
			// The complete wn0G is available from the parser
			wn0G = galToGps->wn0G;
		}
		else
		{
			// The wn0G is truncated because represented by using 6 bits only.
			wn0G = (gstWn & 0xffffffc0) | (galToGps->wn0G & 0x0000003f);
		}
		deltaWn = gstWn - wn0G;
		*gstToGpsTimeOffset = galToGps->a0G + (galToGps->a1G * (gstTow-(double)galToGps->t0G + (double)(SECONDS_IN_A_WEEK*deltaWn)));
		return TRUE_FLAG;
	}
	*gstToGpsTimeOffset = 0.0;
	return FALSE_FLAG;
}


int Sign(double a)
{
	return ((a >= 0.0) ? 1 : -1);
}

double Module(double x, double y)
{
	return (fmod(x, y) + y * (1 - Sign(x)) / 2);
}


/* TODO se io lascio i moduli 86400 che ci sono nell'ICD, il tempo UTC
 * sembra sbagliato. Perche'?
 * Io tolgo il modulo. */

void FromGnssTimeToUtcTime(TGnssToUtcTimeConversion *utcData, int utcDataValid, int defaultLeapSeconds, int gnssWn, double gnssTime, int *utcWn, double *utcTime)
{
	int wn0TUnrolled;
	int wnLsfUnrolled;
	double currTimeComplete;
	double leapTimeComplete;
	double dtUtc;
	double w;

	/* preliminary check of the input arguments */
	if((utcData==NULL) && (utcDataValid==TRUE_FLAG))
	{
		/* invalid pointer, set the flag to FALSE */
		utcDataValid = FALSE_FLAG;
	}

	/* if UTC parameters are not valid, use a constant */
	if(utcDataValid==FALSE_FLAG)
	{
		*utcWn = gnssWn;
		*utcTime = gnssTime - defaultLeapSeconds;
		if(*utcTime < 0)
		{
			*utcTime += SECONDS_IN_A_WEEK;
			*utcWn = *utcWn -1;
		}
		return;
	}

	/* if UTC parameters are valid, use them! */
	// unroll wn0T and wnLsf, taking into account that the difference with the untruncated WN does not exceed 127
	// if wn0T and wnLsf are already complete, the loops should not be executed
	wn0TUnrolled = utcData->wn0T;
	while(abs(gnssWn-wn0TUnrolled)>127)
	{
		wn0TUnrolled += 256;
	}
	wnLsfUnrolled = utcData->wnLsf;
	while(abs(gnssWn-wnLsfUnrolled)>127)
	{
		wnLsfUnrolled += 256;
	}
	/* implement the algorithm described by the Galileo ICD */
	// assume that the leap second is added at the midnight between DN and DN+1
	currTimeComplete = ((double)gnssWn*SECONDS_IN_A_WEEK) + gnssTime;
	leapTimeComplete = ((double)wnLsfUnrolled*SECONDS_IN_A_WEEK) + (utcData->dn*SECONDS_IN_A_DAY);
	//
	*utcWn = gnssWn;
	//if(leapTimeComplete>(currTimeComplete + (6*SECONDS_IN_A_HOUR)))
	if((leapTimeComplete>currTimeComplete) && (fabs(currTimeComplete-leapTimeComplete)>(6*SECONDS_IN_A_HOUR)))
	{
		/* case a
		 * whenever the leap second adjustment time IS NOT in the past and
		 * the user's present time does not fall in the time span which starts six hours prior to the effective time
		 * and ends six hours after the effective time */
		 dtUtc = utcData->dtLs + utcData->a0 + (utcData->a1*(gnssTime-utcData->t0T + (SECONDS_IN_A_WEEK*(gnssWn-wn0TUnrolled))));
		 *utcTime = gnssTime - dtUtc;
		 //*utcTime = Module(gnssTime - dtUtc,86400);
	}
	//else if(leapTimeComplete<(currTimeComplete - (6*SECONDS_IN_A_HOUR)))
	else if((leapTimeComplete<currTimeComplete) && (fabs(currTimeComplete-leapTimeComplete)>(6*SECONDS_IN_A_HOUR)))
	{
		/* case c
		 * whenever the leap second adjustment time IS in the past and
		 * the user's present time does not fall in the time span which starts six hours prior to the leap second adjustment time
		 * and ends six hours after the adjustment time */
		 dtUtc = utcData->dtLsf + utcData->a0 + (utcData->a1*(gnssTime-utcData->t0T + (SECONDS_IN_A_WEEK*(gnssWn-wn0TUnrolled))));
		 *utcTime = gnssTime - dtUtc;
		 //*utcTime = Module(gnssTime - dtUtc,86400);
	}
	else
	{
		/* case b
		 * whenever the the user's current time falls within the time span
		 * of six hours prior to the leap second adjustment time
		 * to six hours after the adjustment time */
		dtUtc = utcData->dtLs + utcData->a0 + (utcData->a1*(gnssTime-utcData->t0T + (SECONDS_IN_A_WEEK*(gnssWn-wn0TUnrolled))));
		w = Module(gnssTime - dtUtc - 43200,86400) + 43200;
		*utcTime = gnssTime - w;
		//*utcTime = Module(w,86400+utcData->dtLsf-utcData->dtLs);
	}

	/* finally adjust tow and week number */
	if((*utcTime)<0)
	{
		// previous week
		(*utcTime) += SECONDS_IN_A_WEEK;
		(*utcWn)--;
	}
	else if((*utcTime)>=SECONDS_IN_A_WEEK)
	{
		// next week
		(*utcTime) -= SECONDS_IN_A_WEEK;
		(*utcWn)++;
	}
}


double TimeDifference(int wn1, double tow1, int wn2, double tow2)
{
	double difference = 0;

	difference  = (double)((wn1 - wn2) * SECONDS_IN_A_WEEK);
	difference += (tow1 - tow2);

	return difference;
}

void TimeIncrement(int *wn, double *tow, int wnIncrement, double towIncrement)
{
	*tow = *tow + towIncrement;
	while(*tow >= (double)SECONDS_IN_A_WEEK)
	{
		*tow -= (double)SECONDS_IN_A_WEEK;
		*wn = *wn +1;
	}
	while(*tow < 0)
	{
		*tow += (double)SECONDS_IN_A_WEEK;
		*wn = *wn -1;
	}
	*wn = *wn + wnIncrement;
	if(*wn < 0)
	{
		*wn = 0;
	}
}


void GetPcTime(TDateTime *currTime)
{
	struct timespec ts;
	struct tm tmVar;

	/* Get the PC time */
	timespec_get(&ts, TIME_UTC);
	tmVar = *gmtime(&ts.tv_sec);
	/* save in the internal format */
	currTime->year = tmVar.tm_year + 1900;
	currTime->month = tmVar.tm_mon + 1;
	currTime->day = tmVar.tm_mday;
	currTime->hour = tmVar.tm_hour;
	currTime->minute = tmVar.tm_min;
	currTime->second = (double)tmVar.tm_sec + ((double)ts.tv_nsec*FROM_NANOSECONDS_TO_SECONDS);
}


void FromPcTimeToGalileoTime(TGnssToUtcTimeConversion *utcData,int utcDataValid,int defaultLeapSeconds,int *wn, double *tow)
{
	TDateTime currTime;
	int utcWn;
	double utcTow;
	double leapSeconds;


	GetPcTime(&currTime);
	SwrxLog(NGENE_LOG_SEVERITY_IMPORTANT_INFO,"NGene2 emulated receiver clock %d/%d/%d %02d:%02d:%02d",currTime.day,currTime.month,currTime.year,currTime.hour,currTime.minute,(int)(currTime.second));

	/* Get UTC time in WN+TOW format (WN valid for Galileo system time!) */
	FromDateToGnssTime(GALILEO_SAT, wn, tow, &currTime);
	/* From UTC to Galileo */
	// find leap seconds
	FromGnssTimeToUtcTime(utcData,utcDataValid,defaultLeapSeconds,*wn,*tow, &utcWn, &utcTow);
	leapSeconds = TimeDifference(*wn, *tow, utcWn, utcTow);
	// apply the leap seconds to find the result
	TimeIncrement(wn, tow, 0, leapSeconds);
}
