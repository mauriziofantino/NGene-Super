/**
 * @file ncursesDataStructure.h
 *
 * @author Mario Nicola
 *
 * @date 26/11/2014
 *
 * @version 1.0.0
 *
 * @brief Output to the NCurses interface
 */

#ifndef UTILITY_NCURSES_DATA_STRUCTURE_H_INCLUDED
#define UTILITY_NCURSES_DATA_STRUCTURE_H_INCLUDED

#include <ncurses.h>

/**
 * @def NCURSES_SLEEP
 * @brief When NCurses interface is used and receiver stops,
 *        NCURSES_SLEEP seconds are waited before closing the NCurses graphics.
 */
#define NCURSES_SLEEP	2


/* Constants used in the output to NCurses */

/* Used to set the line index */
#define NC_ROW_TITLE 			0
#define NC_ROW_DIAMONDS 		1
#define NC_ROW_TIME				2
#define NC_ROW_POSITION_TITLE	4
#define NC_ROW_PRN_TABLE_TITLE	7
#define NC_NEXT_ROW				1
#define NC_EMPTY_ROW 			1

/* Column index for logs */
#define NC_COL_DIAMONDS 		0
#define NC_COL_TIME	            0
#define NC_COL_LATITUDE 		0
#define NC_COL_LONGITUDE 		20
#define NC_COL_ALTITUDE 		40
#define NC_COL_VELOCITY 		60
#define NC_COL_GDOP 			80
#define NC_COL_STATUS			18
#define NC_COL_LOG				0
#define NC_COL_TABLE_CHANNEL	0
#define NC_COL_TABLE_PRN		8
#define NC_COL_TABLE_STATE		20
#define NC_COL_TABLE_CN0		36
#define NC_COL_TABLE_DOPPLER	48
#define NC_COL_TABLE_OSNMA      65
#define NC_COL_TABLE_DSM        100
#define NC_COL_TABLE_KEY        115
#define NC_COL_TABLE_NAVMSG     130

/* Labels */
#define NC_STR_TITLE 		"NGene2"
#define NC_STR_LATITUDE		"Latitude"
#define NC_STR_LONGITUDE	"Longitude"
#define NC_STR_ALTITUDE		"Altitude [m]"
#define NC_STR_VELOCITY		"Velocity [km/h]"
#define NC_STR_GDOP			"GDOP"
#define NC_STR_STATUS		"Receiver status: "
#define NC_STR_TABLE_CHANNEL 	"Ch."
#define NC_STR_TABLE_PRN	 	"PRN"
#define NC_STR_TABLE_STATE 		"State"
#define NC_STR_TABLE_CN0	 	"CN0 [dB-Hz]"
#define NC_STR_TABLE_DOPPLER 	"Doppler [Hz]"
#define NC_STR_TABLE_OSNMA      "OSNMA Last bits and Status"
#define NC_STR_TABLE_DSM        "DSM"
#define NC_STR_TABLE_KEY        "TESLA key"
#define NC_STR_TABLE_NAVMSG     "Nav. message"
#define NC_STR_UNKNOWN 			"--"
#define NC_STR_EMPTY			""
#define NC_STR_PRN_IS_GPS 		"GPS"
#define NC_STR_PRN_IS_GALILEO 	"GAL"
#define NC_STR_PRN_IS_E5A       "E5A"
#define NC_STR_PRN_IS_EGNOS 	"EGN"

/* Channel state strings: must be padded to the longest width */
#define NC_STR_CH_POWERON_STATE			"IDLE            "
#define NC_STR_CH_ACQUISITION_STATE		"ACQUISITION     "
#define NC_STR_CH_TRACKING_STATE		"TRACKING        "
#define NC_STR_CH_NAVIGATION_STATE		"NAVIGATION      "
#define NC_STR_CH_UNUSED_STATE 			"IDLE            "
#define NC_STR_CH_UNHEALTHY_STATE		"UNHEALTHY       "
#define NC_STR_CH_NAVMSG_INVALID_STATE	"INVALID NAV MSG "
#define NC_STR_CH_ERROR_STATE			"ERROR           "
#define NC_STR_CH_AUTHENTICATED_STATE	"NAV MSG AUT     "
#define NC_STR_CH_NOT_AUTH_STATE		"NAV MSG NOT AUT "


/* Receiver state */
#define NC_STR_STATUS_RX_RUNNING 					"RUNNING         "
#define NC_STR_STATUS_RX_STOPPED 					"STOPPED         "

/* Colors */
#define NC_COLOR_SCHEME_NOTE		1
#define NC_COLOR_SCHEME_WARNING		2
#define NC_COLOR_SCHEME_ERROR		3
#define NC_COLOR_SCHEME_TITLE		4
#define NC_COLOR_SCHEME_INFO		5
#define NC_COLOR_SCHEME_DATAOK		6
#define NC_COLOR_SCHEME_ACQ			7
#define NC_COLOR_SCHEME_TRK			8
#define NC_COLOR_SCHEME_NAV			9
#define NC_COLOR_SCHEME_UNHEALTHY	10
#define NC_COLOR_SCHEME_NO_OSNMA    11
#define NC_COLOR_SCHEME_OSNMA_TEST  12
#define NC_COLOR_SCHEME_OSNMA_OP    13
#define NC_COLOR_SCHEME_OSNMA_BAD   14


/**
 * @struct NCursesDataStruct
 * @brief Contains some variables used to manage NCurses interface
 */
typedef struct NCursesDataStruct
{
	WINDOW *uiWin; 				/**< Main User Interface window */
	int colorsEnabled;			/**< Flag indicating whenever the current window supports colors */
	int colsOld;				/**< Number of columns (old value) */
	int linesOld;				/**< Number of rows (old value) */
	int tmpStrLen;				/**< Number of characters in tmpStr */
	char *tmpStr;				/**< String used to store the message to be printed in the NCurses user interface */
	/* index of row for each element in the window follows */
	int rowTitle;				/**< Index of the row containing the name of the receiver */
	int rowDiamonds;			/**< Index of the row containing the number of channels */
	int rowTime;				/**< Index of the row containing the current time */
	int rowPvtTitle;			/**< Index of the row containing the header for the PVT results */
	int rowPvtData;				/**< Index of the row containing the PVT results */
	int rowChannelsTitle;		/**< Index of the row containing the header for the channels section */
	int rowFirstChannelData;	/**< Index of the row containing information about the first channel */
	int rowSettings;			/**< Index of the row containing information about the setting of the receiver */
	int rowStatus;				/**< Index of the row containing the status of the receiver */
	int rowMessages;			/**< Index of the row containing error messages from the receiver */
	int rowCloseHint;			/**< Index of the row containing hint about exiting the receiver */
} TNCursesData;


#endif /* UTILITY_NCURSES_DATA_STRUCTURE_H_INCLUDED */
