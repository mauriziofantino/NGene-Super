/**
 * @file timeConstants.h
 * @author Mario Nicola
 * @date 09-10-2012
 *
 * @brief Some constants related to time information
 */
#ifndef TIME_CONSTANTS_H_INCLUDED
#define TIME_CONSTANTS_H_INCLUDED

/**
 * @define SECONDS_TO_MILLISECONDS
 * @brief Constant used to pass from s to ms
 */
#define SECONDS_TO_MILLISECONDS  1000.0

/**
 * @define FROM_MICROSECONDS_TO_SECONDS
 * @brief Constant used to pass from us to s
 */
#define FROM_MICROSECONDS_TO_SECONDS   1.0e-6

/**
 * @define FROM_NANOSECONDS_TO_SECONDS
 * @brief Constant used to pass from ns to s
 */
#define FROM_NANOSECONDS_TO_SECONDS    1.0e-9

/**
 * @def SECONDS_IN_A_MINUTE
 * @brief Number of seconds in a minute.
 */
#define SECONDS_IN_A_MINUTE   60

/**
 * @def SECONDS_IN_A_HOUR
 * @brief Number of seconds in a minute.
 */
#define SECONDS_IN_A_HOUR   3600

/**
 * @def SECONDS_IN_HALF_DAY
 * @brief Number of seconds in a day.
 */
#define SECONDS_IN_HALF_DAY    43200

/**
 * @def SECONDS_IN_A_DAY
 * @brief Number of seconds in a day.
 */
#define SECONDS_IN_A_DAY    86400

/**
 * @def SECONDS_IN_A_WEEK
 * @brief Number of seconds in a week.
 */
#define SECONDS_IN_A_WEEK	604800

/**
 * @def MINUTES_IN_A_HOUR
 * @brief Number of minutes in a hour.
 */
#define MINUTES_IN_A_HOUR   60

/**
 * @def HOURS_IN_A_DAY
 * @brief Number of hours in a day.
 */
#define HOURS_IN_A_DAY   24

/**
 * @def DAYS_IN_FEBRUARY_NO_LEAP_YEAR
 * @brief Number of days in February (no leap year).
 */
#define DAYS_IN_FEBRUARY_NO_LEAP_YEAR   28

/**
 * @def DAYS_IN_FEBRUARY_LEAP_YEAR
 * @brief Number of days in February (leap year).
 */
#define DAYS_IN_FEBRUARY_LEAP_YEAR   29

/**
 * @def DAYS_IN_A_SHORT_MONTH
 * @brief Number of days in a short month.
 */
#define  DAYS_IN_A_SHORT_MONTH  30

/**
 * @def DAYS_IN_A_LONG_MONTH
 * @brief Number of days in a short month.
 */
#define  DAYS_IN_A_LONG_MONTH  31

/**
 * @def DAYS_IN_A_YEAR
 * @brief Number of days in a year.
 */
#define DAYS_IN_A_YEAR   365

/**
 * @def DAYS_IN_A_LEAP_YEAR
 * @brief Number of days in a leap year (anno bisestile).
 */
#define DAYS_IN_A_LEAP_YEAR   366

/**
 * @def FIRST_GPS_YEAR
 * @brief First year in the GPS time.
 */
#define FIRST_GPS_YEAR   1980

/**
 * @def FIRST_GPS_MONTH
 * @brief First month in the GPS time.
 */
#define FIRST_GPS_MONTH   JANUARY

/**
 * @def FIRST_GPS_DAY
 * @brief First day in the GPS time.
 */
#define FIRST_GPS_DAY   6

/**
 * @def FIRST_GALILEO_YEAR
 * @brief First year in the GALILEO time.
 */
#define FIRST_GALILEO_YEAR   1999

/**
 * @def FIRST_GALILEO_MONTH
 * @brief First month in the GALILEO time.
 */
#define FIRST_GALILEO_MONTH   AUGUST

/**
 * @def FIRST_GALILEO_DAY
 * @brief First day in the GALILEO time.
 */
#define FIRST_GALILEO_DAY   22

/**
 * @def GPS_WN_TO_GAL_WN_BIAS
 * @brief The GPS WN is 1024 in advance w.r.t. the Galileo WN
 */
#define GPS_WN_TO_GAL_WN_BIAS    1024

/**
 * @def ONE_FOURTH
 * @brief Used to represent the six hours (a quarter of a day) margin before (after) a change in the number of leap seconds
 */
#define ONE_FOURTH 0.25

/**
 * @def TWO_POW_7_MINUS_1
 * @brief Maximum signed number that can be represented with 7 bits.
 */
#define TWO_POW_7_MINUS_1   127

/**
 * @def TWO_POW_8
 * @brief Maximum unsigned number that can be represented with 8 bits (+1).
 */
#define TWO_POW_8   256

/**
 * @def TWO_POW_9
 * @brief Maximum unsigned number that can be represented with 9 bits (+1).
 */
#define TWO_POW_9   512

/**
 * @def TWO_POW_10
 * @brief Maximum unsigned number that can be represented with 10 bits (+1).
 */
#define TWO_POW_10   1024

/**
 * @def DEFAULT_UTC_LEAP_SECOND
 * @brief If GNSS to UTC parameters are not known,
 *        this constant is used as leap second correction
 *        18.0 is the value updated on November 19th, 2019.
 */
#define DEFAULT_UTC_LEAP_SECOND	(18.0)


/* define some aliases */
#define SECONDS_PER_MINUTE      (SECONDS_IN_A_MINUTE)
#define SECONDS_PER_HOUR        (SECONDS_IN_A_HOUR)
#define SECONDS_PER_WEEK 		(SECONDS_IN_A_WEEK)



#endif /* TIME_CONSTANTS_H_INCLUDED */
