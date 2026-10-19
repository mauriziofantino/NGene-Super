/**
 * @file timeConversions.h
 * @author Mario Nicola
 * @date 30/10/2014
 *
 * @brief Header file for the time conversions utility
 */
#ifndef TIME_CONVERSIONS_H_INCLUDED
#define TIME_CONVERSIONS_H_INCLUDED

#include "ngeneDataStructure.h"
#include "spreadingCodes.h"
#include "navigationMessageDataStructure.h"

/**
 * @enum MonthEnum
 *
 * @brief is the type that indicates the month.
 */
typedef enum MonthEnum
{
	JANUARY=1, /**< JANUARY = 1 */
	FEBRUARY,  /**< FEBRUARY = 2 */
	MARCH,     /**< MARCH = 3 */
	APRIL,     /**< APRIL = 4 */
	MAY,       /**< MAY = 5 */
	JUNE,      /**< JUNE = 6 */
	JULY,      /**< JULY = 7 */
	AUGUST,    /**< AUGUST = 8 */
	SEPTEMBER, /**< SEPTEMBER = 9 */
	OCTOBER,   /**< OCTOBER = 10 */
	NOVEMBER,  /**< NOVEMBER = 11 */
	DECEMBER   /**< DECEMBER = 12 */
} TMonth;

/**
 * @struct DateTimeStruct
 * @brief Represents the current date and time.
 */
typedef struct DateTimeStruct
{
	int year;      /**< Current year */
	TMonth month;  /**< Current month */
	int day;       /**< Current day in the month (starts from 1) */
	int hour;      /**< Current hour (starts from 0) */
	int minute;    /**< Current minute in the hour (starts from 0) */
	double second; /**< Current second (starts from 0) */
} TDateTime;

/**
 * @brief FromGnssTimeToDate converts a date/time from GNSS format (week number, time of week)
 *        to a real date (dd/mm/yyyy hh:mm:ss)
 *
 * @param system is the constellation to which input time refers
 * @param wn is the input week number
 * @param tow is the input time of week
 * @param datePtr points to the output structure
 */
void FromGnssTimeToDate(TSatConstellation system, int wn, double tow, TDateTime *datePtr);

/**
 * @brief FromDateToGnssTime converts a real date (dd/mm/yyyy hh:mm:ss)
 *        into a GNSS format (week number, time of week)
 *
 * @param system is the constellation to which input time refers
 * @param wn points to the output week number
 * @param tow points to the output time of week
 * @param datePtr points to the input structure
 */
void FromDateToGnssTime(TSatConstellation system, int *wn, double *tow, TDateTime *datePtr);

/**
 * @brief FromDayNumberToDate finds the date for a day whose
 *        number in the year is known. The year is known.
 *
 * @param dayOfTheYear is the index of the day in the current year.
 *        dayOfTheYear must be less than the number of days in the current year.
 * @param datePtr points to the output structure
 *
 * @return 1 if the dayOfTheYear parameter is invalid, 0 else.
 */
int FromDayNumberToDate(int dayOfTheYear, TDateTime *datePtr);

/**
 * @brief FromDateToDayNumber finds the day number from the date.
 *
 * @param datePtr points to the output structure
 *
 * @return the dayOfTheYear.
 */
int FromDateToDayNumber(TDateTime *datePtr);

/**
 * @brief GetGstToGpsTimeOffset translates from Galileo System Time (GST)
 *        to GPS time.
 *
 * @param rx points to the receiver base data structure
 * @param gstWn Week Number in GST.
 * @param gstTow Time Of Week in GST.
 * @param gstToGpsTimeOffset points the offset to be subtracted to GST TOW
 *        in order to find GPS TOW.
 *
 * @return TRUE_FLAG if the offset is valid (needed parameters not found in the
 *         Galileo navigation message), TRUE_FLAG else.
 */
int GetGstToGpsTimeOffset(TNGeneData *rx,int gstWn, double gstTow, double *gstToGpsTimeOffset);

/**
 * @brief FromGnssTimeToUtcTime converts from GNSS time to UTC
 *        (week number, time of week). If parameters are not available,
 *        then a constant is subtracted in order to perform the conversion.
 *        It is based on the algorithm reported in the Galileo ICD.
 *
 * @param utcData points to the parameters used for conversion
 * @param utcDataValid is IS_TRUE when the parameters are valid.
 * @param defaultLeapSeconds default value of leap seconds, read from configuration file
 * @param gnssWn is the input week number
 * @param gnssTime is the input time of week
 * @param utcWn points to the output week number
 * @param utcTime points to the output time of week
 */
void FromGnssTimeToUtcTime(TGnssToUtcTimeConversion *utcData, int utcDataValid, int defaultLeapSeconds, int gnssWn, double gnssTime, int *utcWn, double *utcTime);

/**
 * @brief TimeDifference returns the difference (in seconds) between two GNSS time.
 *
 * @param wn1 is the week number of the time 1
 * @param tow1 is the time of week of the time 1 (in seconds)
 * @param wn2 is the week number of the time 2
 * @param tow2 is the time of week of the time 2 (in seconds)
 *
 * @return time1-time2 in seconds
 */
double TimeDifference(int wn1, double tow1, int wn2, double tow2);

/**
 * @brief TimeIncrement returns the sums between two GNSS time.
 *
 * @param wn points the week number
 * @param tow points to the time of week (in seconds)
 * @param wnIncrement is the week number of the increment
 * @param towIncrement is the time of week of the increment (in seconds)
 */
void TimeIncrement(int *wn, double *tow, int wnIncrement, double towIncrement);

/**
 * @brief GetPcTime reads the date and the time of the PC.
 *
 * @param currTime points to the structure where the current date and time are saved
 */
void GetPcTime(TDateTime *currTime);

/**
 * @brief FromPcTimeToGalileoTime reads the current local time on the PC and uses it to emulate a receiver clock.
 *
 * @details The PC time is UTC (year,month,day,hour,minute,second).
 *          It takes into account both the time zone and the daylight saving time.
 *          The fractional part of the second is ignored
 *          The date and time is translated into Galileo WN/TOW by using the number of LEAP SECONDS recorded in the receiver
 *          (an error in the leap second should be negligible w.r.t. the error in the PC time).
 *
 * @param utcData points to the parameters used for conversion
 * @param utcDataValid is IS_TRUE when the parameters are valid.
 * @param defaultLeapSeconds default value of leap seconds, read from configuration file
 * @param wn points the week number
 * @param tow points to the time of week (in seconds)
 */
void FromPcTimeToGalileoTime(TGnssToUtcTimeConversion *utcData,int utcDataValid,int defaultLeapSeconds,int *wn, double *tow);

#endif /* TIME_CONVERSIONS_H_INCLUDED */
