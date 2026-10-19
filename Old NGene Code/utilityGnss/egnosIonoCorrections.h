/**
 * @file egnosIonoCorrections.h
 *
 * @author Mario Nicola
 *
 * @date 21/08/2015
 *
 * @version 1.0.0
 *
 * @brief Functions used to manage the EGNOS ionospheric corrections.
 */

#ifndef EGNOS_IONO_CORRECTIONS_H_INCLUDED
#define EGNOS_IONO_CORRECTIONS_H_INCLUDED

/**
 * @def VERTICAL_DELAY_LIMIT
 * @brief If verticalDelay is beyond this value [m], then "Do not use" flag is true
 */
#define VERTICAL_DELAY_LIMIT	63.8

/**
 * @struct
 */
typedef struct IgpInfoStruct
{
    int lat[4];					/**< IGP latitude [deg] */
    int lon[4];					/**< IGP longitude [deg] */
    int lat1;
    int lat2;
    int lon1;
    int lon2;
    int bandNumber[4];			/**< band number */
    int maskIndex[4];			/**< IGP index in the mask (INVALID_VALUE if not valid) */
    double verticalDelay[4];	/**< IGP vertical delay [m] */
    int validElements;			/**< number of valid elements in the array (<=4) */
} TIgpInfo;


void GetIgpLatLon(int band, int idx, int *lat, int *lon);

void GetIonosphericPiercePoint(double *userPos,double az,double el,double *ippLat,double *ippLon,double *fpp);

int EgnosIonosphericCorrection(TNGeneConfiguration *cfg,TEgnosCorrections *egnosCorr,double ippLat,double ippLon,double fpp,double *ionoCorrection);

#endif /* EGNOS_IONO_CORRECTIONS_H_INCLUDED */
