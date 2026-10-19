/**
 * @file egnosCorrectionsDataStructure.h
 *
 * @author Mario Nicola
 *
 * @date 05/08/2015
 *
 * @version 1.0.0
 *
 * @brief This header defines constants and types related to the EGNOS corrections.
 */

#ifndef EGNOS_CORRECTIONS_DATA_STRUCTURE_H_INCLUDED
#define EGNOS_CORRECTIONS_DATA_STRUCTURE_H_INCLUDED

#include <stdio.h>
#include "pvtConstants.h"


/**
 * @def EGNOS_ENTRIES
 * @brief The EGNOS message contains corrections for a maximum of 51 satellites
 */
#define EGNOS_ENTRIES	51

/**
 * @def EGNOS_IONO_BANDS
 * @brief The ionosphere model broadcasted by EGNOS is made of 11 bands (0-10)
 */
#define EGNOS_IONO_BANDS	11

/**
 * @def EGNOS_IONO_POINTS
 * @brief The maximum number of points for each band in the EGNOS ionosphere model
 */
#define EGNOS_IONO_POINTS 201


/**
 * @enum
 * @brief
 */
typedef enum EgnosConstellationEnum
{
	INVALID_EGNOS_CONSTELLATION, /**< default invalid value */
	GPS_PRN,			/**< GPS Reserved PRN */
	GLONASS_SLOT,		/**< GLONASS Slot Number plus 37 */
	FUTURE_GNSS,		/**< Future GNSS */
	SBAS_PRN,			/**< GEO/SBAS PRN */
	FUTURE_OTHERS		/**< Future GNSS/GEO/SBAS/Pseudolites */
} TEgnosConstellation;


/**
 * @struct
 * @brief
 */
typedef struct EgnosMaskElementStruct
{
	TEgnosConstellation satType;
	int satId;
} TEgnosMaskElement;


/**
 * @struct
 * @brief
 */
typedef struct EgnosFastCorrectionStruct
{
	char iodp;			/**< IODP (same as in the PRN mask) */
	char iodf;			/**< IODF */
	unsigned char valid;/**< TRUE_FLAG when the corrections are available (both PRC and RRC) */
	double prc;			/**< The PRC */
	unsigned int udrei;	/**< UDREI */
	double towOld;		/**< TOW for the last message of this type */
	char towOldValid;	/**< TRUE_FLAG when towOld is valid */
	double rrc;			/**< RRC */
	/**/
	double udreiMeters;			/**< [m] */
	double sigma2Udre;			/**< [m^2] */
	unsigned char monitored;	/**< FALSE_FLAG if not monitored (UDREI==14) */
	unsigned char toBeUsed;		/**< FALSE_FLAG if cannot be used (UDREI==15) */
} TEgnosFastCorrection;


/**
 * @struct
 * @brief
 */
typedef struct EgnosIntegrityInformationStruct
{
	char iodf;			/**< IODF */
	unsigned char valid;/**< TRUE_FLAG when the information is valid */
	unsigned int udrei;	/**< UDREI */
	/**/
	double udreiMeters;			/**< [m] */
	double sigma2Udre;			/**< [m^2] */
	unsigned char monitored;	/**< FALSE_FLAG if not monitored (UDREI==14) */
	unsigned char toBeUsed;		/**< FALSE_FLAG if cannot be used (UDREI==15) */
} TEgnosIntegrityInformation;


/**
 * @struct
 * @brief
 */
typedef struct EgnosFastCorrectionDegradationFactorStruct
{
	char iodp;			/**< IODP (same as in the PRN mask) */
	unsigned char valid;/**< TRUE_FLAG when the information is valid */
	unsigned char tLat; /**< System latency */
	unsigned int ai;	/**< Degradation factor indicator */
	/**/
	double a; 			/**< Degradation factor [m/(s^2)] */
	int ifc; 			/**< User Time-Out Interval for fast corrections [s] */
	int ifcLp;			/**< User Time-Out Interval for fast corrections [s] - LP approach */
	int updateInterval;	/**< Maximum Fast Correction Update Interval [s] */
} TEgnosFastCorrectionDegradationFactor;


/**
 * @struct
 * @brief
 */
typedef struct EgnosDegradationFactorsStruct
{
	unsigned char valid;
	float bRrc;
	float cLtcLsb;
	float cLtcV1;
	unsigned int iLtcV1;
	float cLtcV0;
	unsigned int iLtcV0;
	float cGeoLsb;
	float cGeoV;
	unsigned int iGeo;
	float cEr;
	float cIonoStep;
	unsigned int iIono;
	float cIonoRamp;
	unsigned int rssUdre;
	unsigned int rssIono;
	float cCovariance;
} TEgnosDegradationFactors;


/**
 * @struct
 * @brief
 */
typedef struct EgnosLongTermCorrectionsStruct
{
	char iodp;
	unsigned char validVelocityCode0;
	unsigned char validVelocityCode1;
	int iod;
	double deltaX;
	double deltaY;
	double deltaZ;
	double deltaAf0;
	double rateDeltaX;
	double rateDeltaY;
	double rateDeltaZ;
	double deltaAf1;
	double to;
} TEgnosLongTermCorrections;


/**
 * @struct
 * @brief
 */
typedef struct EgnosIonoMaskStruct
{
	char maskIodi;
	unsigned char numOfPointInTheMask;
	unsigned char pointIndexes[EGNOS_IONO_POINTS];
	/**/
	int pointLatitudes[EGNOS_IONO_POINTS];
	int pointLongitudes[EGNOS_IONO_POINTS];
} TEgnosIonoMask;


/**
 * @struct
 * @brief
 */
typedef struct EgnosIonoIgpDelayCorrectionsStruct
{
	char iodi;
	unsigned char valid;
	double igpVerticalDelay;
	int igpGivei;
	/**/
	unsigned char usable;
	double giveMeters;
	double sigma2Give;
	unsigned char monitored;
} TEgnosIonoIgpDelayCorrections;


/**
 * @struct
 * @brief
 */
typedef struct EgnosIonoBandDelayCorrectionsStruct
{
	TEgnosIonoIgpDelayCorrections igpIonoCorrections[EGNOS_IONO_POINTS];
} TEgnosIonoBandDelayCorrections;


/**
 * @struct
 * @brief
 */
typedef struct EgnosCorrectionsStruct
{
	/* PRN mask */
	char maskIodp;								/**< Two bits flag for the PRN mask */
	int numOfPrnsInTheMask;						/**< Number of PRNs in the PRN mask */
	TEgnosMaskElement prnMask[EGNOS_ENTRIES];	/**< All the PRNs in the PRN mask */
	/* fast corrections */
	TEgnosFastCorrection fastCorrections[EGNOS_ENTRIES];
	/* integrity information */
	TEgnosIntegrityInformation integrityInformation[EGNOS_ENTRIES];
	/* fast correction degradation factor */
	TEgnosFastCorrectionDegradationFactor fastDegradationFactors[EGNOS_ENTRIES];
	/* degradation factors */
	TEgnosDegradationFactors degradationFactors;
	/* long term satellite error correction */
	TEgnosLongTermCorrections longTermCorrections[EGNOS_ENTRIES];
	/* ionosphere */
	char numOfIonoBands;			/**< Number of broadcasted bands */
	char numOfReceivedIonoBands;	/**< Number of received bands */
	TEgnosIonoMask ionoMask[EGNOS_IONO_BANDS];
	TEgnosIonoBandDelayCorrections bandIonoCorrections[EGNOS_IONO_BANDS];
	FILE *ionoDebugFile;
} TEgnosCorrections;

#endif /* EGNOS_CORRECTIONS_DATA_STRUCTURE_H_INCLUDED */
