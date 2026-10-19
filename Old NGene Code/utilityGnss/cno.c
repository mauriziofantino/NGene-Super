/**
 * @file cno.c
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
#include <math.h>
#include "cno.h"
#include "mathFunctions.h"
#include "ngeneConstants.h"

#define CARRIER_QUANTIZATION_CORRECTION 0.8


/**< squared value of a */
#define SQUARE(a) (a*a)
/**< squared module of a complex number */
#define MODULE_SQUARE(a,b) (a*a+b*b)


// Prepare the table for the CNo measure
void SetCnoMeasurement(TCnoTable *cno, double iStart, double qStart, int size, int cnoSize, double lastCno)
{
    // Set the moving average filters, at the time being the filter memory is set equal to zero
	InitAverageWindow(&(cno->m2), size, MODULE_SQUARE(iStart, qStart));
	InitAverageWindow(&(cno->m4), size, SQUARE(MODULE_SQUARE(iStart, qStart)));

    // Initialize the CNo moving average filter if necessary
    cno->cnoSize = cnoSize;
    if(cnoSize)
    {
    	InitAverageWindow(&(cno->cnoW), cnoSize, lastCno);
    }

    return;
}

// Function to estimate the Carrier to Noise Power Spectral Density Ratio with Moments method and Hatch filters - NL
double MeasureCno(TCnoTable *cno, double iSample, double qSample,TNGeneConfiguration *cfg)
{
    double ps;			/* Signal averaged power */
    double pn;			/* Net power */
    double cOverNo;		/* Carrier To Noise estimated value */
    double m2;			/* Second order moment */
    double m4;			/* Fourth order moment */

    // Evaluation of the second and fourth order moments
    ps = MODULE_SQUARE(iSample, qSample);
    m2 = PerformAverageWindow(&(cno->m2), ps);
    m4 = PerformAverageWindow(&(cno->m4), SQUARE(ps));

    // Evaluation of the signal and noise power
    ps = sqrt(2 * SQUARE(m2) - m4);
    pn = m2 - ps;

    if(pn >0)
    {
    	cOverNo = 10 * log(ps / pn * cno->beq) / log(10.0);
    	cOverNo += CARRIER_QUANTIZATION_CORRECTION;

    	// smooth the Carrier To Noise Power Spectral Density if required
    	if(cno->cnoSize)
    		return PerformAverageWindow(&(cno->cnoW), cOverNo);
    	else
    		return cOverNo;
    }
    else
    {
    	return PerformAverageWindow(&(cno->cnoW), (double)cfg->trkConfiguration.cutoffCno/2.0);
	}
}

void ComputeSigmaJ(TCnoTable *cno, double value)
{
	double sigmaJ2;

	PerformAverageWindow(&(cno->x2), value*value);
	PerformAverageWindow(&(cno->muX), value);

	sigmaJ2 = GetAverageWindowValue(&(cno->x2)) - GetAverageWindowValue(&(cno->muX))*GetAverageWindowValue(&(cno->muX));
	cno->sigmaJ2 = sigmaJ2;

}

double ComputeEquivalentBandwidth(TCnoTable *cno, double tchip, int blocksize, double rcFactor)
{
	double sigma2;
	double rc;
	double tchip2;

	tchip2 = tchip * tchip; /*tchip^2*/
	sigma2 = cno->sigmaJ2*tchip2; /*variance of the code delay tracking error estimated after the code loop filter*/
	//**
	rc = sqrt(2.0*sigma2/M_PI) * (1-exp(-tchip2/(2.0*sigma2)));
	rc = 1.0 - ((rcFactor*rc)/tchip);
	cno->beq = (double)cno->samplingFrequency/(rc*rc*(double)blocksize);

	return cno->beq;
}
