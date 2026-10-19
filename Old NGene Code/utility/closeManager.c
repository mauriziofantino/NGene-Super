/**
 * @file closeManager.c
 *
 * @author Mario Nicola
 *
 * @date 07/08/2014
 *
 * @version 1.0.0
 *
 * @brief Declares a function that closes the receiver.
 */

#include <unistd.h>
#include <stdlib.h>
#include "ngeneConfiguration.h"
#include "ngeneConstants.h"
#include "ncursesDataStructure.h"
#include "ncursesApi.h"



void CloseNGene(TNGeneConfiguration *cfg,int returnValue)
{
	switch(cfg->userInterface)
	{
		case RX_UI_NCURSES:
			sleep(NCURSES_SLEEP);
			if(cfg->ncursesDataInitialized==TRUE_FLAG)
			{
				DeinitNCurses(cfg->ncursesData);
			}
			break;
		case RX_UI_TEXT:
			break;
		case RX_UI_GUI:
			sleep(GUI_CLIENT_TIMEOUT);
			break;
	}

	exit(returnValue);
}


