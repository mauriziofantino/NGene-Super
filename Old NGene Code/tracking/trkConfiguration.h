/**
 * @file trkConfiguration.h
 *
 * @author Mario Nicola
 *
 * @date 07/08/2014
 *
 * @version 1.0.0
 *
 * @brief This header defines the structure that stores NGene2 configuration for TRK
 *        from both GUI and configuration files.
 */

#ifndef TRK_CONFIGURATION_H_INCLUDED
#define TRK_CONFIGURATION_H_INCLUDED


/**
 * @struct TrkConfigurationStruct
 *
 * @brief Contains the NGene2 receiver configuration for tracking.
 *
 * @details Tracking loop parameters (bandwidths and gains) are related to the
 *          employed tracking model (from Borre-Akos)
 */
typedef struct TrkConfigurationStruct
{
	float cutoffCno;					/**< Satellites under this limit are not tracked [dBHz] */
	/* Specific GPS parameters */
	int gpsIntegrationPeriod;			/**< GPS integration period chosen by the user [ms] */
	float gpsDllBw;						/**< GPS bandwidth of the DLL [Hz] */
	float gpsDllGain;					/**< GPS gain of the DLL */
	float gpsFllBw;						/**< GPS bandwidth of the FLL [Hz] */
	float gpsFllGain;					/**< GPS gain of the FLL */
	float gpsPllBw;						/**< GPS bandwidth of the PLL [Hz] */
	float gpsPllGain;					/**< GPS gain of the PLL */
	float gpsSpacing;					/**< GPS early-late spacing [chip] */
	float veryEarlyVeryLateGpsSpacing;					/**< GPS very early-very late spacing [chip] */
	float veryEarlyVeryLateEgnosSpacing;				/**< EGNOS very early-very late spacing [chip] */
	float veryEarlyVeryLateGalileoSpacing;				/**< Galileo very early-very late spacing [chip] */
	/* Specific EGNOS parameters */
	int egnosIntegrationPeriod;			/**< EGNOS integration period chosen by the user [ms] */
	float egnosDllBw;					/**< EGNOS bandwidth of the DLL [Hz] */
	float egnosDllGain;					/**< EGNOS gain of the DLL */
	float egnosFllBw;					/**< EGNOS bandwidth of the FLL [Hz] */
	float egnosFllGain;					/**< EGNOS gain of the FLL */
	float egnosPllBw;					/**< EGNOS bandwidth of the PLL [Hz] */
	float egnosPllGain;					/**< EGNOS gain of the PLL */
	float egnosSpacing;					/**< EGNOS early-late spacing [chip] */
	/* Specific Galileo parameters */
	float galileoDllBw;					/**< Galileo bandwidth of the DLL [Hz] */
	float galileoDllGain;				/**< Galileo gain of the DLL */
	float galileoFllBw;					/**< Galileo bandwidth of the FLL [Hz] */
	float galileoFllGain;				/**< Galileo gain of the FLL */
	float galileoPllBw;					/**< Galileo bandwidth of the PLL [Hz] */
	float galileoPllGain;				/**< Galileo gain of the PLL */
	float galileoSpacing;				/**< Galileo early-late spacing [chip] */
	/* Parameters used for the FLL/PLL transition */
	int fllPllChoiceFilterLen;			/**< Length of the Hatch filter used to manage FLL to PLL transition */
	int fllPllChoiceFilterUpdateTime;	/**< Time between two consecutive attempts to pass from FLL to PLL [ms] */
	float fllPllThreshold;				/**< Threshold used to FLL to PLL decision */
	/* Parameter used to activate user integration period (GPS) */
	int changePeriodCtrlLength;			/**< GPS: how many data transition must be found before activating the user integration period */
	/* multiple correlators */
	int multipleCorrelatorsFlag;		/**< multiple correlators are used */
	int numberOfMultipleCorrelators;	/**< number of multiple correlators to be used */
	int multipleCorrelatorsGpsSpacing;		/**< spacing between multiple correlators [samples] for GPS*/
	int multipleCorrelatorsGalileoSpacing;	/**< spacing between multiple correlators [samples] for Galileo*/
	int multipleCorrelatorsEgnosSpacing;	/**< spacing between multiple correlators [samples] for EGNOS*/
} TTrkConfiguration;





#endif /* TRK_CONFIGURATION_H_INCLUDED */
