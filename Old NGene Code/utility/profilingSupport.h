/**
 * @file profilingSupport.h
 *
 * @author Mario Nicola
 *
 * @date 09/08/2019
 *
 * @version 1.0.0
 *
 * @brief Variables and functions used to add profiling information
 */


#ifndef PROFILING_SUPPORT_H_INCLUDED
#define PROFILING_SUPPORT_H_INCLUDED

#include <sys/time.h>
#include <time.h>
#include "timeConstants.h"

/**
 * @def OSNMA_PROFILING
 * @brief If defined, the profiling of the OSNMA functions is performed
 */
//#define OSNMA_PROFILING

/**
 * @def CHIMERA_PROFILING
 * @brief If defined, the profiling of the CHIMERA functions is performed
 */
//#define CHIMERA_PROFILING


/**
 * @def RX_PROFILING
 * @brief If defined, the profiling of acquisition, refinement and tracking is performed
 */
//#define RX_PROFILING

#define TARGET_NAME_MAX_LENGTH	     256
#define INVALID_PROFILING_TIME        -1.0

#define ENABLE_NANOSECOND_PRECISION
#define PROFILING_RESULT_FILENAME    "profilingResults.csv"


/* supporting data structure */
typedef struct ProfilingSupportStruct
{
	char targetName[TARGET_NAME_MAX_LENGTH];
	/**/
	unsigned int numberOfExecutions;
	unsigned int numberOfExecutionsOld;
	double totalTimeS;
	double startTimeS;
	double endTimeS;
	/**/
	#ifdef ENABLE_NANOSECOND_PRECISION
	struct timespec tv;
	#else
	struct timeval tv;
	#endif
} TProfilingSupport;


/* used time values structures */
//struct timeval {
//  time_t tv_sec; /* seconds */
//  suseconds_t tv_usec; /* microseconds */
//};

//struct timespec {
//	time_t tv_sec; /* seconds */
//	long tv_nsec; /* nanoseconds */
//};



/* supporting functions */
void InitProfilingSupport(TProfilingSupport *prof,char *targetName);
void StartProfilingUpdate(TProfilingSupport *prof);
void StopProfilingUpdate(TProfilingSupport *prof,unsigned int numberOfExecutions);
void ReportProfilingData(TProfilingSupport *prof);
void TotalTimeProfilingUpdate(TProfilingSupport *prof);


#endif /* PROFILING_SUPPORT_H_INCLUDED */
