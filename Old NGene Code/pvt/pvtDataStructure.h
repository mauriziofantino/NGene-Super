/**
 * @file pvtDataStructure.h
 *
 * @author Mario Nicola
 *
 * @date 22/10/2014
 *
 * @version 1.0.0
 *
 * @brief This header defines the data structure used for PVT computation.
 */

#ifndef PVT_DATA_STRUCTURE_H_INCLUDED
#define PVT_DATA_STRUCTURE_H_INCLUDED

#include <stdio.h>
#include "channelDataStructure.h"
#include "pvtConstants.h"
#include "nequickDataStructures.h"


/**
 * @struct SatStatusStruct
 * @brief This structure stores information about the position and the velocity of each satellite
 */
typedef struct SatStatusStruct
{
	int satInformationValid;	/**< TRUE_FLAG is the data are valid */
    double ecefSatPos[3];		/**< x [m], y [m], z [m] in ECEF reference system */
    double ecefSatPosRot[3];	/**< x [m], y [m], z [m] in ECEF reference system AFTER the earth rotation compensation */
    double ecefSatVel[3];		/**< vx [m/s], vy [m/s], vz [m/s] in ECEF reference system */
    double ecefSatVelTot;		/**< Modulus of the satellite velocity [m/s] */
    double azimuth;				/**< Azimuth of the satellite with respect the last receiver position [radian] */
    double elevation;			/**< Elevation of the satellite with respect the last receiver position [radian] */
    double ippLatitude;			/**< Ionospheric Pierce Point latitude [deg] */
    double ippLongitude;		/**< Ionospheric Pierce Point longitude [deg] */
    double fpp;                 /**< Obliquity factor */
} TSatStatus;


/**
 * @struct PvtChannelStruct
 * @brief Stores all the PVT information related to a single channel
 */
typedef struct PvtChannelStruct
{
	TChannel *ch; 				/**< Points to the channel data structure */
	TSatStatus *satStatus;      /**< Points to the sat status structure in the receiver */
	int wn; 					/**< Week Number */
	double transmissionTime; 	/**< Transmission time [s] */
	double satClkCorr;       	/**< Correction of the satellite clock [s] */
	double deltaPseudorange;	/**< Doppler frequency [Hz] */
	double pseudorangeRaw; 	 	/**< Raw pseudorange [m] */
	double pseudorange; 		/**< Pseudorange after clock correction [m] */
	double pseudorangeCsm; 		/**< Pseudorange after smoothing [m] */
	double tropoCorrection;		/**< Troposphere Correction [m] */
	double ionoCorrection;		/**< Ionosphere correction [m] */
	char ionoCorrectionFromEgnos; /**< Flag indicating if the iono correction are get from EGNOS */
	double egnosFastCorrection;	  /**< Egnos pseudorange correction [m] */
	double egnosClockCorr;		  /**< Egnos long term clock correction [s] */
	double egnosXCorr;			  /**< Egnos long term ECEF x position correction [m] */
	double egnosYCorr;			  /**< Egnos long term ECEF y position correction [m] */
	double egnosZcorr;			  /**< Egnos long term ECEF z position correction [m] */
	double pseudorangeResidual;   /**< Pseudorange residual [m] */
	/* Kalman data */
	double posSigma2; 			/**<  */
	double velSigma2; 			/**<  */
} TPvtChannel;


/**
 * @struct LeastSquaresDataStruc
 * @brief Variables used to perform Least Square based PVT.
 * @details This structure stores also variable which don't maintain the value
 *          between consecutive executions, but this avoids that such variables are
 *          allocated on-the-fly in the Least Square function.
 *          Variable whose size changes depending on the number of channel/single or multiple constellation
 *          are sized in order to take into account the worst case.
 */
typedef struct LeastSquaresDataStruct
{
	double *a; /**<  */
	double *at; /**< a transpose */
	double atA[LS_MULTI_SIZE*LS_MULTI_SIZE]; /**< a_Transpose_By_a */
	double invAtA[LS_MULTI_SIZE*LS_MULTI_SIZE]; /**< inverseOf_a_Transpose_By_a */
	double atB[LS_MULTI_SIZE*1]; /**< a_Transpose_By_b */
	double pos[LS_MULTI_SIZE*1];
	double sol[LS_MULTI_SIZE*1];
	double *ac; /**< aCorrect */
	double *act; /**< aCorrect_Transpose */
	double actAc[4*4]; /**< aCorrect_Transpose_By_aCorrect */
	double invActAc[4*4];  /**< inverseOf_aCorrect_Transpose_By_aCorrect */
	double actB[4*1]; /**< aCorrect_Transpose_By_b */
	double sol2[4*1];
	double userVel[4*1];
	double satVel[3*1];
	double distVect[1*3];
	double distance;
	double *satRadialVelAndDoppler;
	double *x;
	double *xRotate;
	double *b;
	double *az;
	double *el;
	double matrixL[LS_MULTI_SIZE*LS_MULTI_SIZE];
	double matrixD[LS_MULTI_SIZE*LS_MULTI_SIZE];
	double matrixU[LS_MULTI_SIZE*LS_MULTI_SIZE];
	double matrixUt[LS_MULTI_SIZE*LS_MULTI_SIZE]; /**< matrixU_Transpose */
	double matrixIL[LS_MULTI_SIZE*LS_MULTI_SIZE];
	double matrixID[LS_MULTI_SIZE*LS_MULTI_SIZE];
	double matrixIU[LS_MULTI_SIZE*LS_MULTI_SIZE];
	double matrixIUt[LS_MULTI_SIZE*LS_MULTI_SIZE]; /**< matrixIU_Transpose */
	double matrixIUD[LS_MULTI_SIZE*LS_MULTI_SIZE];
	double matrixDet[LS_MULTI_SIZE*LS_MULTI_SIZE];
	double dop[5*5];
	double q[5*5];
} TLeastSquaresData;


/**
 * @struct KalmanFilterDataStruct
 * @brief Variables used to perform Kalman filter based PVT.
 * @details This structure stores also variable which don't maintain the value
 *          between consecutive executions, but this avoids that such variables are
 *          allocated on-the-fly in the Kalman filter function.
 *          Variable whose size changes depending on the number of channel/single or multiple constellation
 *          are sized in order to take into account the worst case.
 */
typedef struct KalmanFilterDataStruct
{
	/* Variables to be maintained between consecutive executions */
	double phi[KF_SINGLE_STATES*KF_SINGLE_STATES];		/**< Kalman state transition matrix for the single constellation case */
	double phiMulti[KF_MULTI_STATES*KF_MULTI_STATES];	/**< Kalman state transition matrix for the multiple constellations case */
	double pPost[KF_SINGLE_STATES*KF_SINGLE_STATES];  	/**< pAPostCovariance: a posteriori error covariance matrix used in the single constellation case */
	double pPostMulti[KF_MULTI_STATES*KF_MULTI_STATES];	/**< pAPostCovarianceMultiConst: a posteriori error covariance matrix used in the multiple constellation case */
	double q[KF_SINGLE_STATES*KF_SINGLE_STATES];			/**< qStateNoiseCovariance: noise covariance matrix used in the single constellation case */
	double qMulti[KF_MULTI_STATES*KF_MULTI_STATES]; 	/**< qStateNoiseCovarianceMulti: noise covariance matrix used in the multiple constellation case */
	/* Variables used in the Kalman filter main function */
	double *aaa;
	double *az;
	double *el;
	double *r; /**< rObservationNoiseCovariance; */
	double *range;
	double *deltaRange;
	double *u;
	double *pr; /**< pdr */
	double *prCorr;
	double *deltaPrCorr;
	double *zCurr; /**< zCurrentMeasurement; */
	double *nominalMeasurement;
	double *kfMeasurement;
	double *h; /**< hObservationMatrix; */
	double stateAPosteriori[KF_MULTI_STATES*1];
	/* Variables used for matrix inversion */
	double *matrixL;
	double *matrixD;
	double *matrixU;
	double *matrixUt; /**< matrixU_Transpose */
	double *matrixIL;
	double *matrixID;
	double *matrixIU;
	double *matrixIUt; /**< matrixIU_Transpose */
	double *matrixIUD;
	double *matrixDet;
	/* Variables used in Kalman Filter prediction and Kalman Filter routine */
	double xPred[KF_MULTI_STATES*1]; /**< xPredictedState */
	double pPriori[KF_MULTI_STATES*KF_MULTI_STATES];
	double phiPap[KF_MULTI_STATES*KF_MULTI_STATES]; /**< phiPapAPostCovariance */
	double phit[KF_MULTI_STATES*KF_MULTI_STATES]; /**< phiStateTransitMatrix_Transpose */
	double phiPapPhit[KF_MULTI_STATES*KF_MULTI_STATES]; /**< phiStateTransitMatrix_By_PAPostCovariance_By_PHIStateTransitMatrix_Transpose */
	/* Variables used in Kalman Filter routine */
	double *kGain;
	double *ht; /**< hObservationMatrix_Transpose */
	double *pprioriHt; /**< pAPrioriCovariance_By_HObservationMatrix_Transpose */
	double *hPpriori; /**< hObservationMatrix_By_PAPrioriCovariance */
	double *hPprioriHt; /**< hObservationMatrix_By_PAPrioriCovariance_By_HObservationMatrix_Transpose */
	double *hPprioriHtR; /**< hObservationMatrix_By_PAPrioriCovariance_By_HObservationMatrix_Transpose_Plus_RObservationNoiseCovariance */
	double *invHPprioriHtR; /**< inverseOf_HObservationMatrix_By_PAPrioriCovariance_By_HObservationMatrix_Transpose_Plus_RObservationNoiseCovariance */
	double xCurr[KF_MULTI_STATES*1]; /**< xCurrentEstimState */
	double *hXpred; /**< hObservationMatrix_By_XPredictedState */
	double *zcurrHXpred; /**< zCurrentMeasurement_Minus_HObservationMatrix_By_XPredictedState */
	double kgainZcurrHXpred[KF_MULTI_STATES*1]; /**< kGain_By_ZCurrentMeasurement_Minus_HObservationMatrix_By_XPredictedState */
	double i[KF_MULTI_STATES*KF_MULTI_STATES]; /**< identityXPreviousEstimStateLength */
	double kgainH[KF_MULTI_STATES*KF_MULTI_STATES]; /**< kGain_By_HObservationMatrix */
	double iKgainH[KF_MULTI_STATES*KF_MULTI_STATES]; /**< identityXPreviousEstimStateLength_Minus_KGain_By_HObservationMatrix */
	double iKgainHPpriori[KF_MULTI_STATES*KF_MULTI_STATES]; /**< identityXPreviousEstimStateLength_Minus_KGain_By_HObservationMatrix_By_PAPrioriCovariance */
	double iKgainHT[KF_MULTI_STATES*KF_MULTI_STATES]; /**< identityXPreviousEstimStateLength_Minus_KGain_By_HObservationMatrix_Transpose */
	double *kgainR; /**< kGain_By_RObservationNoiseCovariance */
	double *kgaint; /**< kGain_Transpose */
	double kgainRKgaint[KF_MULTI_STATES*KF_MULTI_STATES]; /**< kGain_By_RObservationNoiseCovariance_By_KGain_Transpose */
	double aB[KF_MULTI_STATES*KF_MULTI_STATES]; /**< a_By_B */
} TKalmanFilterData;


/**
 * @struct CarrierSmoothingDataStruct
 * @brief Variables used to perform Carrier Smoothing (CSM) over measured pseudoranges.
 */
typedef struct CarrierSmoothingDataStruct
{
	double csmMeasure;	/**< Measure from tracking loop */
	double lpc;			/**<  */
	double lpcOut;		/**< Temporary variable for the output (LPC in cycles) */
	double lpcOld;		/**< LPC at the previous PVT */
	double pseudorangeCsmOld;   /**< Pseudorange after smoothing, from the previous PVT computation [m] */
	int csmCounter;		/**< Counter of smoothing executions for the current channel */
} TCarrierSmoothingData;

/**
 * @enum ReceiverTimeFlagEnum
 * @brief Indicates constellation and validity for a time
 */
typedef enum ReceiverTimeFlagEnum
{
	INVALID_TIME=0,
	UTC_TIME=1,
	GPS_TIME=2,
	GALILEO_TIME=3
} TReceiverTimeFlag;

/**
 * @struct PvtDataStruct
 * @brief Stores all the PVT information common to all the channels
 */
typedef struct PvtDataStruct
{
	unsigned long long int pvtIndex;/**< Index of the computed PVTs*/
	int lsIndex; 					/**< How many LS-based PVTs have been performed */
	int pvtAlgorithm; 				/**< LS or KF */
	int rxWnForNextPvt;				/**< Receiver WN when next PVT will be evaluated */
	double rxTowForNextPvt; 		/**< Receiver TOW when next PVT will be evaluated */
	TReceiverTimeFlag rxTimeForNextPvtFlag;	/**< Flag for rxTimeForNextPvt */
	/**/
	int numOfPvtFails;          /**< How many times the PVT results have been discarded
	                                 by the PVT check due to large errors.
	                                 It is 0 if the last PVT was valid or no PVT has been computed yet */
	int hardRestartMade;        /**< If a maximum number of PVT failures is reached, then all the channels
	                                 and related information are restarted */
	/**/
	int minNumOfChannels;       /**< Minimum number of channels required to perform next PVT */
	int numOfGpsOpChannels;		/**< Number of GPS channels ready to be used for PVT */
	int numOfGalileoOpChannels;	/**< Number of Galileo channels ready to be used for PVT */
	int numOfOperativeChannels;	/**< Number of channels used PVT */
	int numOfChannelsReadyForPVT;	/**< Number of channels ready to be used for PVT */
	int numOfGpsChannelsReadyForPVT;	/**< Number of GPS channels ready to be used for PVT */
	int numOfGalileoChannelsReadyForPVT;	/**< Number of Galileo channels ready to be used for PVT */
	TPvtChannel *pvtChArray;	/**< Information specific to each channel */
	/* Carrier smoothing */
	TCarrierSmoothingData *csmData; /**< Information about carrier smoothing (for each channel */
	int csmTransient; 				/**< if TRUE_FLAG, the CarrierSmoothing is in the initial transient for all the channels */
	unsigned long long int csmTransientStart;	/**< if TRUE_FLAG, the CarrierSmoothing is in the initial transient for all the channels
	                                                 This is the PVT index at the start of the CSM transient.  */
	double weightCode;			/**< Weight used to add the code measure with the carrier one */
	double weightCarrier;		/**< Weight used to add the carrier measure with the code one */
	/* Current time */
	int rxGpsWn;				/**< Current GPS WN for the receiver */
	double rxGpsTow;			/**< Current GPS TOW for the receiver */
	int rxGpsTimeValid;			/**< If TRUE_FLAG, rxGpsTow is valid */
	int rxGalWn;				/**< Current Galileo WN for the receiver */
	double rxGalTow;			/**< Current Galileo TOW for the receiver */
	int rxGalTimeValid;			/**< If TRUE_FLAG, rxGalTow is valid */
	int rxUtcWn;				/**< Current UTC WN for the receiver */
	double rxUtcTow;			/**< Current UTC TOW for the receiver */
	int rxUtcTimeValid;			/**< If TRUE_FLAG, rxUtcTow is valid */
	/* NeQuick model */
	TNeQuickData nequick;
	// the following arrays contain info about Nequick corrections for Galileo (first elements of the array) and GPS (last elements)
	double *nequickCorrections;		/**< Computed Nequick ionospheric corrections for Galileo (plus GPS), one per each possible satellite */
	double *nequickCorrectionsTime;	/**< Time (s from the start of elaboration) when Nequick Iono correction have been evaluated */
	/* PVT algorithms */
	TLeastSquaresData lsData;   /**<  */
	TKalmanFilterData kfData;	/**<  */
	/**/
	FILE *pvtLogFile; 			/**< File containing an optional text log of PVT computation */
	FILE *pvtBinFile; 			/**< File containing an optional binary log of PVT computation */
	FILE *gnssTimeLogFile;      /**< File containing an optional text log of the estimated receiver time (GNSS time) */
} TPvtData;


/**
 * @struct ReceiverTimeStruct
 * @brief Contains data used to store the time of the receiver
 *        as corrected by the PVT routine.
 */
typedef struct ReceiverTimeStruct
{
	int referenceValid;						/**< TRUE_FLAG if the reference is from a valid PVT */
	int referenceUpdated;                   /**< TRUE_FLAG if the last PVT updated the reference time */
	unsigned long long int referenceSample;	/**< index (in the file) of the samples used as the reference */
	int referenceWn;						/**< week number (receiver time) of the reference sample */
	double referenceTow;					/**< time of week (receiver time) of the reference sample [s].
	                                              It's floating point because takes into account also the fractional part */
} TReceiverTime;


/**
 * @struct PvtResultsStruct
 * @brief This structure stores PVT results: user (position, velocity, receiver time)
 *        and each satellite (position, velocity)
 */
typedef struct PvtResultsStruct
{
	int pvtValid;				/**< TRUE_FLAG when the position is valid */
	int pvtPlotted;				/**< TRUE_FLAG when the current position has already been displayed to the user */
	unsigned long long int referenceSample;	/**< index (in the file) of the samples used as the reference */
	int clockBiasGpsValid;		/**< TRUE_FLAG when the clock bias for GPS satellites is valid */
	int clockBiasGalileoValid;	/**< TRUE_FLAG when the clock bias for GPS satellites is valid */
	double ecefPosition[3];		/**< User's position in ECEF reference system: x [m], y [m], z[m] */
	double ecefVelocity[3];		/**< User's velocity in ECEF reference system: vx [m/s], vy [m/s], vz [m/s] */
	double geodPosition[3];		/**< User's position in geodetic reference system lon (degree), lat (degree), h [m] */
	double utmZone;				/**< UTM zone */
	double enuPosition[3]; 		/**< User's position in ENU reference system [m] */
	double enuVelocity[3]; 		/**< User's velocity in ENU reference system [m/s] */
	double clockBiasGps;		/**< Bias of the receiver clock for GPS [m] */
	double clockBiasGalileo;	/**< Bias of the receiver clock for Galileo [m] */
	double clockBiasGpsCumulated;	/**< clockBiasGps accumulated over consecutive executions [m] */
	double clockBiasGalileoCumulated;	/**< clockBiasGalileoBias accumulated over consecutive executions [m] */
	double clockDrift;			/**< Drift of the receiver clock [m/s] */
	double dop[5]; 				/**< GDOP, PDOP, HDOP, VDOP, TDOP */
	TReceiverTime rxGpsTime;	/**< Estimated GPS time of the receiver */
	TReceiverTime rxGalTime;	/**< Estimated Galileo time of the receiver */
	int utcFromGps;				/**< If TRUE_FLAG, UTC is found from the GPS time */
	int utcFromGal;				/**< If TRUE_FLAG, UTC is found from the Galileo time */
	int utcFromDefault;         /**< If TRUE_FLAG, UTC is found by using the default value of leap seconds */
	TReceiverTime rxUtcTime;	/**< Estimated UTC time of the receiver */
	TSatStatus *gpsSatStatus;	/**< Information about position and velocity of all GPS satellites */
	TSatStatus *galSatStatus;	/**< Information about position and velocity of all Galileo satellites */
	FILE *kmlFile; 				/**< Optional pointer to the KML file used for PVT log */
} TPvtResults;


#endif /* PVT_DATA_STRUCTURE_H_INCLUDED */
