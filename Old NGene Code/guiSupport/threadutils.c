/* 
 * @file threadutils.c
 * @author Marracci (Intecs)
 *
 * @brief Created on December 11, 2007, 2:18 PM
 */

#include <pthread.h>
#include <stdio.h>
#include "ioFunctions.h"
#include "ngeneConfiguration.h"


int generate_pthread ( pthread_t *my_pthread_id, void *(*my_pthread_func)(void *), int my_priority)
{
  
  pthread_attr_t			my_pthread_attr;
  struct sched_param	my_pthread_param;
  
  int ret;
  
  // Set scheduling policy
  pthread_attr_init (&my_pthread_attr);
  ret = pthread_attr_setschedpolicy (&my_pthread_attr, SCHED_FIFO);
  if (ret != 0)
  {
	  SwrxLog(NGENE_LOG_SEVERITY_ERROR,"error: pthread_attr_setschedpolicy");
	  return (ret);
  }
  
  // Set priority
  my_pthread_param.sched_priority = my_priority;
  pthread_attr_setschedparam (&my_pthread_attr, &my_pthread_param);
  if (ret != 0)
  {
	  SwrxLog(NGENE_LOG_SEVERITY_ERROR, "error: pthread_attr_setschedparam");
	  return (ret);
  }
  
  // Create thread
  ret = pthread_create (my_pthread_id, NULL, my_pthread_func, NULL);
  if (ret != 0)
  {
	  SwrxLog(NGENE_LOG_SEVERITY_ERROR, "error: pthread_create");
	  return (ret);
  }
  
  return 0;
}
