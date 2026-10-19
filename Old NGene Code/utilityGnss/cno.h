/**
 * @file cno.h
 *
 * @author Mario Nicola
 *
 * @date 20/03/2012
 *
 * @version 1.0.0
 *
 * @brief Nicola Linty implementation of the C over N0 estimator
 *
 */

#ifndef CNO_H_INCLUDED
#define CNO_H_INCLUDED

#include "mathFunctions.h"
#include "ngeneConfiguration.h"


/**
 * @def GALILEO_CN0_CORRECTION_FACTOR
 * @brief It is the correction factor [dB] to apply to the Galileo C/N0 estimation,
 * since the C/N0 is estimated only on the E1B band, but the whole band is received and the power is
 * distributed accordingly
 */
#define GALILEO_CN0_CORRECTION_FACTOR 6.5

/**
 * @def GPS_CN0_CORRECTION_FACTOR
 * @brief It is the correction factor [dB] to apply to the GPS C/N0 estimation,
 * since the C/N0 is estimated only on the C/A band, but the whole band is received
 * (C/A+P+NAV) and the power is distributed accordingly
 */
#define GPS_CN0_CORRECTION_FACTOR 1.7


/**
 * @struct Cn0TableStruct
 *
 * @brief  Data structure used to estimate the C over N0
 */
typedef struct CnoTableStruct
{
	/* used to calculate equivalent bandwidth */
	double sigmaJ2;       /**< Variance of the code discriminator output */
	double dllBandwidth; /**<  */
	TAverageWindow x2;   /**<  */
	TAverageWindow muX;  /**<  */
	unsigned int samplingFrequency;	/**< */
	double estimatedCNo; /**< */
	/* other variables */
	double beq;			 /**< Correlator Noise Equivalent Bandwidth */
    TAverageWindow cnoW; /**< moving average window for the Carrier to Noise Power Density Ratio */
    TAverageWindow m2;	 /**< moving average window for the second order moment */
    TAverageWindow m4;	 /**< moving average window for the fourth order moment */
    int cnoSize;		 /**< size of the smoothing filter */
} TCnoTable;

/**
 * @brief Prepare the table for the CNo measure
 *
 * @param cno is the pointer to the CNo structure
 * @param iStart is the filter memory for the I branch moving average filter
 * @param qStart is the filter memory for the Q branch moving average filter
 * @param size is the size of the moving average filter
 * @param cnoSize is the size of the moving average filter to smooth the \f$ \frac{C}{N_0}\f$,
 *        when it is equal to zero any filter is applied on
 * @param lastCno is the last evaluated CNo value
 */
void SetCnoMeasurement(TCnoTable *cno, double iStart, double qStart, int size, int cnoSize, double lastCno);


/**
 * @brief Function to estimate the Carrier to Noise Power Spectral Density Ratio
 *
 * @details This estimator has been firstly proposed for a BPSK modulation, while
 *          its extension to the QPSK modulation reduces to the ML algorithm. An
 *          intuitive modification has been introduced and tested here for QPSK
 *          modulations, leaving the BPSK solution unchanged:
 *          \f[ \tilde{P}_c=\left[\frac{1}{N}\displaystyle\sum_{n=1}^N\left|r_{C,Re}[n]\right|\right]^2
 *                         +\left[\frac{1}{N}\displaystyle\sum_{n=1}^N\left|r_{C,Im}[n]\right|\right]^2\f]
 *
 *          \f[ \tilde{P}_{tot}=\frac{1}{N}\displaystyle\sum_{n=1}^N\left|r_C[n]\right|^2\f]
 *
 *          \f[ \tilde{P}_n=\tilde{P}_{tot}-\tilde{P}_s\f]
 *
 *          \f[ \tilde{SNR}_C=\frac{\tilde{P}_s}{\tilde{P}_n}=\frac{\tilde{P}_s}{\tilde{P}_{tot}-\tilde{P}_s}\f]
 *
 *          The novel approach suffers for a non-negligible bias in case of complex
 *          samples, but it results more robust than ML solution in case of reduced
 *          number of available samples \f$ N\f$.
 *
 * @param cno is the pointer to the CNo structure
 * @param iSample is the current I prompt correlator sample
 * @param qSample is the current Q prompt correlator sample
 * @param cfg points to the receiver configuration
 */
double MeasureCno(TCnoTable *cno, double iSample, double qSample,TNGeneConfiguration *cfg);

/**
 * @brief Function to compute the variance of the code delay tracking error
 * @param cno is the pointer to the CNo structure
 * @param value is the variable for which to compute the variance
 */
void ComputeSigmaJ(TCnoTable *cno, double value);

/**
 * @brief Function to compute the equivalent noise bandwidth for the C/N0 estimation
 *
 * @details The cno->beq is computed as reported in:
 * Falletti, E.; Pini, M.; Presti, L.L.,
 * "Low Complexity Carrier-to-Noise Ratio Estimators for GNSS Digital Receivers,"
 * Aerospace and Electronic Systems, IEEE Transactions on ,
 * vol.47, no.1, pp.420,437, January 2011
 *
 * cno->beq=cno->samplingFrequency/(rc^2*blocksize);
 * where rc=1-(rcFactor*sqrt(2*sigma^2/PI)*(1-exp(-tchip^2/(2.0*sigma^2))))/tchip
 * and sigma^2=code delay tracking error variance expressed in [s^2]
 * IN the NEW VERSION CURRENTLY IN USE:
 * sigma^2 is estimated AFTER THE CODE LOOP FILTER, in this way:
 * ComputeSigmaJ(&(trk->trkStatusChecker.cnoEstimator), codeDelayErrorPerTChip)
 * where codeDelayErrorPerTChip=(ch->codeRate-GPS_CODE_RATE)*tchip
 * thus sigma^2=cno->sigmaJ2*tchip^2
 *
 *
 * IN the OLD VERSION :
 * sigma^2 was estimated AFTER THE CODE DISCRIMINATOR (DLL), in this way:
 * ComputeSigmaJ(&(trk->trkStatusChecker.cnoEstimator), trk->codeLoop.dllDiscriminator);
 * thus sigma^2=cno->sigmaJ2 /Kd^2*M
 * where M=2*cno->dllBandwidth*tint and Kd=-2*rcFactor/tchip/SECONDS_TO_MILLISECONDS
 *
 * THE TWO VERSION ARE EQUIVALENT
 *
 * @param cno is the pointer to the CNo structure
 * @param tchip is the chip time (17codeRate)
 * @param blocksize is the number of samples in 1 ms
 * @param  rcFactor is a correction factor to be applied to ideal code autocorrelation function rc: 1 for GPS, 3 for Galileo
 */
double ComputeEquivalentBandwidth(TCnoTable *cno, double tchip, int blocksize, double rcFactor);

#endif /* CNO_H_INCLUDED */
