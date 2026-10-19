/**
 * @file profilingSupport.c
 *
 * @author Mario Nicola
 *
 * @date 09/08/2019
 *
 * @version 1.0.0
 *
 * @brief Variables and functions used to add profiling information
 */

#include <stdio.h>
#include "profilingSupport.h"


void InitProfilingSupport(TProfilingSupport *prof,char *targetName)
{
	sprintf(prof->targetName,"%s",targetName);
	prof->numberOfExecutions = 0;
	prof->numberOfExecutionsOld= 0;
	prof->totalTimeS = 0.0;
	prof->startTimeS = INVALID_PROFILING_TIME;
	prof->endTimeS = INVALID_PROFILING_TIME;
	// prof->tv overwritten at each execution
}


void StartProfilingUpdate(TProfilingSupport *prof)
{
	#ifdef ENABLE_NANOSECOND_PRECISION
	clock_gettime(CLOCK_MONOTONIC, &(prof->tv));
	prof->startTimeS = prof->tv.tv_sec + (prof->tv.tv_nsec*FROM_NANOSECONDS_TO_SECONDS);
	#else
	gettimeofday(&(prof->tv),NULL);
	prof->startTimeS = prof->tv.tv_sec + (prof->tv.tv_usec*FROM_MICROSECONDS_TO_SECONDS);
	#endif
}


void StopProfilingUpdate(TProfilingSupport *prof,unsigned int numberOfExecutions)
{
	if(prof->startTimeS==INVALID_PROFILING_TIME)
	{
		return;
	}

	if(numberOfExecutions>0)
	{
		/* update counters only if the number of executions is greater than 0 */
		#ifdef ENABLE_NANOSECOND_PRECISION
		clock_gettime(CLOCK_MONOTONIC, &(prof->tv));
		prof->endTimeS = prof->tv.tv_sec + (prof->tv.tv_nsec*FROM_NANOSECONDS_TO_SECONDS);
		#else
		gettimeofday(&(prof->tv),NULL);
		prof->endTimeS = prof->tv.tv_sec + (prof->tv.tv_usec*FROM_MICROSECONDS_TO_SECONDS);
		#endif

		prof->totalTimeS += (prof->endTimeS-prof->startTimeS);
		prof->numberOfExecutionsOld=prof->numberOfExecutions;
		prof->numberOfExecutions +=  numberOfExecutions;
	}
	else
	{
		prof->numberOfExecutionsOld=prof->numberOfExecutions;
	}
	/**/
	prof->startTimeS = INVALID_PROFILING_TIME;
	prof->endTimeS = INVALID_PROFILING_TIME;
}


void ReportProfilingData(TProfilingSupport *prof)
{
	FILE *profFile;
	double timeUs;

	if((prof->numberOfExecutions==0) || (prof->totalTimeS==0.0))
	{
		/* no valid results to be printed */
		return;
	}

	timeUs = (prof->totalTimeS/(double)prof->numberOfExecutions)/FROM_MICROSECONDS_TO_SECONDS;
	profFile = fopen(PROFILING_RESULT_FILENAME,"at");
	if(ftell(profFile)==0)
	{
		/* file is empty, print a header */
		fprintf(profFile,"Target name,Number of executions,Total time [s],Time per execution [us]\n");
	}
	fprintf(profFile,"%s,%u,%f,%f\n",prof->targetName,prof->numberOfExecutions,prof->totalTimeS,timeUs);
	fclose(profFile);
}


void TotalTimeProfilingUpdate(TProfilingSupport *prof)
{
	if(prof->startTimeS==INVALID_PROFILING_TIME)
	{
		return;
	}

	#ifdef ENABLE_NANOSECOND_PRECISION
	clock_gettime(CLOCK_MONOTONIC, &(prof->tv));
	prof->endTimeS = prof->tv.tv_sec + (prof->tv.tv_nsec*FROM_NANOSECONDS_TO_SECONDS);
	#else
	gettimeofday(&(prof->tv),NULL);
	prof->endTimeS = prof->tv.tv_sec + (prof->tv.tv_usec*FROM_MICROSECONDS_TO_SECONDS);
	#endif

	prof->totalTimeS += (prof->endTimeS-prof->startTimeS);

}
