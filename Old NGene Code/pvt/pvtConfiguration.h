/**
 * @file pvtConfiguration.h
 *
 * @author Mario Nicola
 *
 * @date 07/08/2014
 *
 * @version 1.0.0
 *
 * @brief This header defines the structure that stores NGene2 configuration for PVT
 *        from both GUI and configuration files.
 */

#ifndef PVT_CONFIGURATION_H_INCLUDED
#define PVT_CONFIGURATION_H_INCLUDED

#include "navigationMessageDataStructure.h"

#define LMS_BASED_PVT 		0
#define KALMAN_BASED_PVT	1


#define MINIMUM_CSM_RESTART_PERIOD  3


/**
 * @enum PvtAuthSelEnum
 *
 * @brief Represents the possible PVT selection strategies based based on the status of the OSNMA authentication.
 */
typedef enum PvtAuthSelEnum
{
	PVT_DISCARD_NOT_AUTHENTICATED = 0,       /**< Use only satellites with successful authentication */
	PVT_DISCARD_AUTHENTICATION_FAILURES = 1, /**< Discard only satellites with failed authentications */
	PVT_DO_NOT_DISCARD = 2                   /**< Do not use authentication to select satellites */
} TPvtAuthSel;


/**
 * @enum RedCedInPvtEnum
 *
 * @brief Enable/disable the use of RedCED in PVT computation
 */
typedef enum RedCedInPvtEnum
{
	RED_CED_DISABLED = 0, /**< In PVT, use only CED */
	RED_CED_ENABLED = 1,  /**< In PVT, use RedCED when CED are not available (default) */
	RED_CED_ONLY = 2      /**< In PVT, use only RedCED */
} TRedCedInPvt;


/**
 * @struct PvtConfigurationStruct
 *
 * @brief Contains the NGene2 receiver configuration for PVT.
 */
typedef struct PvtConfigurationStruct
{
	float pvtRate;			/**< Update rate of PVT [Hz] */
	float maxGdop;			/**< Maximum Geometric Dilution Of Precision */
	int pvtMethod;			/**< PVT computation method */
	int csmFilterLen;		/**< Length of the carrier smoothing filter (0 = no Carrier Smoothing) */
	int elevationMaskFlag;	/**< Flag for elevation mask use */
	float elevationMask;	/**< If elevationMaskFlag==TRUE_FLAG, it is the elevation mask in radians */
	int urbanCanyonMaskFlag;  /**< Flag for Urban Canyon mask use */
	int egnosFlag;		      /**< EGNOS is used for PVT computation */
	int staticReceiver;       /**< TRUE_FLAG if the receiver is static, FALSE_FLAG otherwise */
	TPvtAuthSel pvtAuthSel;   /**< Which strategy is used to select satellites for PVT in function of authentication */
	TRedCedInPvt redCedInPvt; /**< Enable/disable use of RedCED in PVT */

	/* default Iono models */
	int ionoDefaultValid;  /**< Validity flag for ionoDefault  */
	TGnssIono ionoDefault; /**< Default values for the iono models parameters, read from cfg file */
} TPvtConfiguration;





#endif /* PVT_CONFIGURATION_H_INCLUDED */
