/**
 * @file signalHandler.c
 *
 * @author Maurizio Fantino
 *
 * @date 26/07/2007
 *
 * @version 1.0.0
 *
 * @brief Handle the signal communication from GUI, OS and USERS
 *
 * @note This file is based on Linux standard functions (see man sigaction)
 *
 */

#include <signal.h>
#include <pthread.h>
#include "commModule.h"
#include "ioFunctions.h"
#include "ngeneConfiguration.h"
#include "ngeneConstants.h"
#include "signalHandler.h"

struct sigaction action;	///< The receiver must be able to catch a signal

/** Mutex for the status variable */
extern pthread_mutex_t status_mutex;
/** Condition on status for threads synchronization */
extern pthread_cond_t status_cond;
/** The current status of the Communication module */
extern enum ComStatus_t status;

static TNGeneConfiguration *signalHandlerCfg;
static int *signalHandlerRun;


void StartListeningSignals(TNGeneConfiguration *cfg,int *run)
{
    /* sigaction must before set if a signal has to be masked and how it has to be treated
       then a link between the signal and the corresponding action is created */
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    action.sa_handler = SignalHandler;
    /* Set the pointer to the loop variable (already initialized to TRUE_FLAG) */
    signalHandlerRun = run;
    /* Set the pointer to the receiver configuration */
    signalHandlerCfg = cfg;
    /* now it is time to start listening */
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGQUIT, &action, NULL);
    sigaction(SIGABRT, &action, NULL);
    sigaction(SIGKILL, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGSTOP, &action, NULL);

    return;
}


void SignalHandler(int signol)
{
    *signalHandlerRun = FALSE_FLAG;			///< the receiver is stopped

    switch (signol)
    {
    	case SIGINT:
    		SwrxLog(NGENE_LOG_SEVERITY_WARNING, "Catched signal SIGINT, receiver terminated by the user");
    		break;
    	case SIGQUIT:
    		SwrxLog(NGENE_LOG_SEVERITY_WARNING, "Catched signal SIGQUIT, receiver quit");
    		break;
    	case SIGABRT:
    		SwrxLog(NGENE_LOG_SEVERITY_WARNING, "Catched signal SIGABRT, receiver aborted by the user");
    		break;
    	case SIGKILL:
    		SwrxLog(NGENE_LOG_SEVERITY_WARNING, "Catched signal SIGKILL, receiver killed");
    		break;
    	case SIGTERM:
    		SwrxLog(NGENE_LOG_SEVERITY_WARNING, "Catched signal SIGTERM, receiver terminated");
    		break;
    	case SIGSTOP:
    		SwrxLog(NGENE_LOG_SEVERITY_WARNING, "Catched signal SIGSTOP, receiver stopped");
    		break;
    	default:
    		SwrxLog(NGENE_LOG_SEVERITY_WARNING, "Signal catched and action executed");
    		break;
    }

    if(signalHandlerCfg->userInterface==RX_UI_GUI)
    {
    	pthread_mutex_lock(&status_mutex);
    	status = EXITING;
    	pthread_cond_broadcast(&status_cond);
    	pthread_mutex_unlock(&status_mutex);
    }

    return;
}


