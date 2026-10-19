/**
 * @file usbFrontEndConfiguration.h
 *
 * @author Mario Nicola
 *
 * @date 08/08/2014
 *
 * @version 1.0.0
 *
 * @brief This header defines the structure that stores NGene2 configuration for USB FE.
 *        Parameters for all possible FEs are stored.
 */

#ifndef USB_FRONT_END_CONFIGURATION_H_INCLUDED
#define USB_FRONT_END_CONFIGURATION_H_INCLUDED

#include "ngeneConstants.h"

/* Each index is used to indicate a supported front-end
 * INFO These numbers must be equal to those reported in the Front_End.conf configuration file
 * and those sent by the GUI */
#define NGENE_FE_STEREO			0
#define NGENE_FE_SIGE			1
#define NGENE_FE_CUSTOM   		2
#define NGENE_FE_NFUELS			3
#define NGENE_FE_FRAUNHOFER		4
#define NGENE_FE_USRP			5
#define NGENE_FE_SIGE_V3        6
#define NGENE_FE_STEREO_TRITON  7
#define NGENE_FE_AMUNGO_NT1065  8
#define NGENE_FE_JRC            9

/**
 * @struct usbFeParsStruct
 * @brief Contains parameters for one of the possible FEs.
 */
typedef struct usbFeParsStruct {
    int index;
    char name[MAX_ROW_LENGTH];
    float intermediateFrequency;	/**< \f$ f_{IF} \f$ */
    float samplingRate;					/**< \f$ f_s \f$ */
    int dopplerCorrection;			/**< +1/-1 depending on the FE frequency plan */
    unsigned int resolution;			/**< Resolution in number of used bit of each sample */
    unsigned int meanValue;			/**< If unsigned char samples are used, mean value is grater than 0.
	                                     NGene must convert to signed char, subtracting the mean value */
    int vid;							/**< Vendor ID of the USB front end */
    int pid;							/**< Product ID of the USB front end */
} TUsbFePars;

/**
 * @struct usbFeParametersStruct
 *
 * @brief
 */
typedef struct usbFeConfigurationStruct {
    int numberOfSupportedFes;
    TUsbFePars *feList;
} TUsbFeConfiguration;



#endif				/* USB_FRONT_END_CONFIGURATION_H_INCLUDED */
