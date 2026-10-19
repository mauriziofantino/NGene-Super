/* 
 * @file threadutils.h
 * @author Marracci (Intecs)
 *
 * @brief Created on December 11, 2007, 2:18 PM
 */

#ifndef _THREADUTILS_H
#define	_THREADUTILS_H

#include <pthread.h>
#include "ngeneConfiguration.h"

int generate_pthread ( pthread_t *my_pthread_id, void *(*my_pthread_func)(void *), int my_priority);

#endif	/* _THREADUTILS_H */

