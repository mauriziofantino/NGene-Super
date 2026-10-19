/**
 * @file argvParser.c
 *
 * @author Mario Nicola
 *
 * @date 06/08/2014
 *
 * @version 1.0.0
 *
 * @brief Functions used to parse the command line arguments.
 *        From a previous implementation by Maurizio Fantino.
 */
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include "argvParser.h"
#include "ngeneConfiguration.h"
#include "ioFunctions.h"
#include "closeManager.h"

extern char *optarg;		///< required by the getopt_long linux function used in handle_cmdline

// It simply gets the executable file name and it stores the value
// in the corresponding place in the receiver data structure
void GetMainName(char *argv[], TNGeneConfiguration *cfg)
{
    /* Get the name of the main file as it has been given from the command line
     * Deletes folder(s) and store just the name of the executable file. */
	cfg->progname = (const char *) strrchr(argv[0], '/');
	cfg->progname = cfg->progname ? (cfg->progname + 1) : argv[0];
}

// This function handle the parameters given as command line to the executable file
// and it sets the corresponding variable and receiver data structure
int HandleCmdLine(int argc, char *argv[], TNGeneConfiguration *cfg)
{
    /* Add here more options if necessary */
    struct option opts[] = {
    		{"version", 0,0,'V'},
    		{"textual", 0, 0, 't'},
    		{"ncurses", 0, 0, 'n'},
    		{"port", 1, 0, 'p'},
    		{"googleEarth", 1, 0, 'g'},
    		{"logfile", 1, 0, 'l'},
    		{"verbose", 0, 0, 'v'},
    		{"credits", 0, 0, 'c'},
    		{"help", 0, 0, 'h'},
    		{NULL, 0, 0, 0} };
    // and its relative help output
    const char *optsHelp[] = {
    		"Print the actual version of NGene receiver.",
    		"Use the textual interface (No GUI).",
    		"Use the ncurses terminal interface.",
    		"Set the COM port for the communication with the GUI (default 4999).",
    		"Record the receiver output as a GoogleEarth file.",
    		"Log all the operation a logging file.",
    		"Verbose output.",
    		"Print the credits.",
    		"Print this message." };
    struct option *opt;
    const char **hlp;
    int max, size;
    int i;

    /* Setting default values for the flags */
    cfg->userInterface = RX_UI_GUI;
    cfg->guiComPort = DEFAULT_GUI_TCP_PORT; // GUI COM PORT
    cfg->stdlog = stdout;
    pthread_mutex_init(&(cfg->stdLogMutex),NULL);
    cfg->logFilename[0] = '\0';
    cfg->googleEarthFilename[0] = '\0';
    cfg->googleEarthFilenameValid = FALSE_FLAG;
    cfg->verbose = FALSE_FLAG;
    cfg->configurationValid = FALSE_FLAG;
    /* Parse options */
    while (1)
    {
    	i = getopt_long(argc, argv, "Vtnp:g:l:vcrh", opts, NULL);	// This line has to be modified to handle an optional parameter
    	if(i == -1)
    	{
    		break;
    	}
    	switch (i)
    	{
    		case 'V':
    			cfg->userInterface = RX_UI_TEXT;
    			fprintf(stdout, "\n\nNGene Software Receiver %s \"%s\"\n",NGENE_VERSION_INDEX,NGENE_VERSION_NAME);
    			fprintf(stdout, "Copyright (c) 2007-2025 Navigation Signal Analysis and Simulation group - NavSAS\n\n");
    			exit(EXIT_SUCCESS);
    			break;
    		case 't':
    			cfg->userInterface = RX_UI_TEXT;
    			break;
    		case 'n':
    			cfg->userInterface = RX_UI_NCURSES;
    			break;
    		case 'p':
    			sscanf(optarg, "%u", &cfg->guiComPort);
    			break;
    		case 'g':
    			// Get the googleEarth file name
    			cfg->googleEarthFilename[0] = '\0';
    			strcat(cfg->googleEarthFilename, optarg);
    			cfg->googleEarthFilenameValid=TRUE_FLAG;
    			break;
    		case 'l':
    			// Get the log file name and path
    			cfg->logFilename[0] = '\0';
    			strcat(cfg->logFilename, optarg);
    			break;
    		case 'v':
    			cfg->verbose = TRUE_FLAG;
    			break;
    		case 'c':
    			cfg->userInterface = RX_UI_TEXT;
    			fprintf(stdout, "\n                   GPS/Galileo NGene Software Receiver\n\n");
    			fprintf(stdout, "Copyright (c) 2007-2025 Navigation Signal Analysis and Simulation group - NavSAS\n\n");
    			fprintf(stdout, "                                ;.\n");
    			fprintf(stdout, "                       :cl,    ,x,\n");
    			fprintf(stdout, "                      .kkd'    ox'\n");
    			fprintf(stdout, "                      ;kkk:    do.\n");
    			fprintf(stdout, "                      dkcxk'  .ko.\n");
    			fprintf(stdout, "                     .ko'.kd. 'kc.\n");
    			fprintf(stdout, "                     :k:. ,kl cx,\n");
    			fprintf(stdout, "                .,;:cxd:;,.;kdkl. .dddddl.       ',::cc::;'.\n");
    			fprintf(stdout, "               ,ccc:cd:cccc;;ol, .xkkkkkko.    .cccc:;,;cccc.\n");
    			fprintf(stdout, "               ;cccc;:,';,''.    okkkcokkkc    .cccc:,..,,,''\n");
    			fprintf(stdout, "                ':ccccccc:,.    :kkko;.dkkk;    .;cccccccc:'\n");
    			fprintf(stdout, "                 ...';;:cccc'  ,kkkkl:,lkkkx'    .,:,,;:cccc:.\n");
    			fprintf(stdout, "              .cclc,   'cccc;..kkkkkkkkkkkkkd. ,cdkx,  .;cllc'\n");
    			fprintf(stdout, "               .:cccc::ccccc'.dkkkdc;;;;;xkkkl  ,dkxc::clxxc:.\n");
    			fprintf(stdout, "                 .,;;:::;,'. .:ccc;.     .:ccc'  :kd:::;lkl'\n");
    			fprintf(stdout, "                                                 'kd'  .xo,\n");
    			fprintf(stdout, "                                                  xx,  od;\n");
    			fprintf(stdout, "                                                  :kc.dd:.\n");
    			fprintf(stdout, "                                                   oxxd;\n");
    			fprintf(stdout, "                                                   :kd:.\n");
    			fprintf(stdout, "                                                   .'.\n");

    			exit(EXIT_SUCCESS);
    			break;
    		case 'h':
    			cfg->userInterface = RX_UI_TEXT;
    			fprintf(stdout, "\n         NGene Software Receiver %s \"%s\"\n\n",NGENE_VERSION_INDEX,NGENE_VERSION_NAME);
    			fprintf(stdout, "Copyright (c) 2007-2025 Navigation Signal Analysis and Simulation group - NavSAS\n\n");
    			fprintf(stdout, "Usage: %s [OPTIONS]\n\n", cfg->progname);
    			max = 0;
    			for(opt = opts; opt->name; opt++)
    			{
    				size = strlen(opt->name);
    				if(size > max)
    					max = size;
    			}
    			for(opt = opts, hlp = optsHelp; opt->name; opt++, hlp++)
    			{
    				fprintf(stdout, "  -%c, --%s", opt->val, opt->name);
    				size = strlen(opt->name);
    				for(; size < max; size++)
    					fprintf(stdout, " ");
    				fprintf(stdout, "  %s\n", *hlp);
    			}
    			fprintf(stdout,"\n");
    			exit(EXIT_SUCCESS);
    			break;
    		default:
    			cfg->userInterface = RX_UI_TEXT;
     			fprintf(stderr,"Use -h for help\n");
    			exit(EXIT_FAILURE);
    			break;
    	}
    }
    return SUCCESS_RETURN;
}
