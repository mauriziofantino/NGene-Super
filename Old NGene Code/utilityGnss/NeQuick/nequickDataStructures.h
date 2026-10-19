/**
 * @file    nequickDataStructures.h
 *
 * @author  Mario Nicola
 * @author  Gianluca Falco
 *
 * @date    09/01/2015
 *
 * @version 1.0.0
 *
 * @brief   Include types used in the NeQuick model.
 *
 * @details Codes from the reference NeQuick model
 *          (AF_RTMCMasterConstants.h, BasicTypes.h, NeqTypes.h)
 */

#ifndef NEQUICK_NEQUICK_DATA_STRUCTURES_H_INCLUDED
#define NEQUICK_NEQUICK_DATA_STRUCTURES_H_INCLUDED

#include "nequickConstants.h"

/**
 * @enum ReturnValueEnum
 * @brief Function returned type
 */
typedef enum ReturnValueEnum
{
  E_OK = 0,   /**< No error */
  E_ERROR     /**< Error    */
} TReturnValue;

/**
 * @typedef TBoolean.
 * @brief Type representing a boolean value. A boolean variable can take two
 *  possible values: TRUE or FALSE.
 */
typedef unsigned short TBoolean;


/**
 * @enum ModipRegionEnum
 * @brief This type represents the MODIP region (output of the NeQuick algorithm)
 */
typedef enum ModipRegionEnum
{
	UNKNOWN_REGION = 0,			/**<  MODIP region not valid */
	NORTHERN_REGION = 1,   		/**<  60 deg < MODIP <  90 deg */
	NORTHERN_MIDDLE_REGION = 2,	/**<  30 deg < MODIP <  60 deg  */
	EQUATORIAL_REGION = 3,		/**< -30 deg < MODIP <  30 deg  */
	SOUTHERN_MIDDLE_REGION = 4,	/**< -60 deg < MODIP < -30 deg */
	SOUTHERN_REGION = 5			/**< -90 deg < MODIP < -60 deg */
} TModipRegion;


/**
 * @struct ModipStruct
 * @brief Used to save the map of dip latitude loaded from a parameter file
 */
typedef struct ModipStruct
{
  double pdModip[NUM_MODIP_DIM_1_SI][NUM_MODIP_DIM_2_SI]; /**< A grid point map of dip latitude, Range:-90.0...+90.0, Units: degrees */
} TModip;


/**
 * @struct CcirStruct
 * @brief Content of the CCIR file
 */
typedef struct CcirStruct
{
 double pdF2[MONTHS_SI][CCIR_F2_DIM_1_SI][CCIR_F2_DIM_2_SI][CCIR_F2_DIM_3_SI]; /**< F2 ionogram parameters for Nequick model, Range: NA,  Units: NA */
 double pdM3000[MONTHS_SI][CCIR_M3000_DIM_1_SI][CCIR_M3000_DIM_2_SI][CCIR_M3000_DIM_3_SI]; /**< M3000 ionogram parameters for Nequick model, Range: NA, Units: NA */
} TCcir;


/**
 *  @struct NeQuickInputDataStruct
 *  @brief Input data
 */
typedef struct NeQuickInputDataStruct
{
  TModip *pstModip;   	/**< Config: Array for Modip grid values */
  TCcir *pstCCIR;     	/**< Config: Structure containing CCIR maps */
  double *pdKronrodTol; /**< Tolerances for the Kronrod integration */
  int siMaxRecurse;     /**< Maximum level of integration recursion allowed */
  double *pdGssPosLLH;  /**< GSS position in WGS-84 ellipsoidal coordinates: geodetic latitude, geodetic longitude and ellipsoidal height.*/
  double *pdSatPosLLH;  /**< Satellite position in WGS-84 ellipsoidal coordinates: geodetic latitude, geodetic longitude and ellipsoidal height.*/
  int siMonth;          /**< The current month (january=1, december=12) */
  double dUT;           /**< The universal time (GMT) */
  int siNumCoeff;       /**< Number of items in the pdCoeff array */
  double *pdCoeff;      /**< Array containing the Az or a0,a1,a2... values */
} TNeQuickInputData;


typedef struct SPointStruct
{
  double dLat;          /**< Latitude (deg. N) */
  double dLng;          /**< Longitude (deg. E) */
  double dH;            /**< Height (km) */
  double dR;            /**< Radius (km) */
  double dS;            /**< Slants (km) */
  double dSinLat;       /**< Sin of latitude point */
  double dCosLat;       /**< Cosine of latitude point */
} TSPoint;


typedef struct CurrentCcirStruct
{
  int siMonth;          /**< Month for interpolated values */
  double dR12;          /**< Average sunspot number for interpolated values */
  double pdF0F2[NUM_CCIR_F2_PTS_SI];  /**< Interpolated foF2 (F2-peak plasma frequency) values for this month/R12
   	   	   	   	   	   	   	   	   	   * which is closely related to F2-peak density (NmF2) by the formula:
   	   	   	   	   	   	   	   	   	   * NmF2 = 1.24x10^10 * foF2 */
  double pdM3000F2[NUM_CCIR_M3000_PTS_SI]; 	/**< Interpolated M(3000)F2 values for this month/R12 which =MUF(3000)/foF2 and
   	   	   	   	   	   	   	   	   	   	   	 * is a propagation factor closely related to the height of the F2-peak.
   	   	   	   	   	   	   	   	   	   	   	 * MUF(3000) is the highest frequency that, refracted in the ionosphere, can
   	   	   	   	   	   	   	   	   	   	   	 * be received at a distance of 3000km. */
  double dUT;           					/**< Time for current Spherical Legendre coefficients */
  double pdLegCoeffs_F0[CCIR_F2_DIM_2_SI];  /**< Spherical Legendre coeffs for calculating FoF2 */
  double pdLegCoeffs_M3000[CCIR_M3000_DIM_2_SI];	/**< Spherical Legendre coeffs for calculating M3000F2 */
} TCurrentCcir;


typedef struct LayerPropertiesStruct
{
  double pdAmp[LAYER_COUNT_SI];			/**< Amplitude for the Epstein function */
  double pdPeakHeight[LAYER_COUNT_SI];
                            			/**< Hm, height of the electron density peak for the
                            			 * F2, F1 and E layers respectively */
  double pdBotThick[LAYER_COUNT_SI];	/**< Thickness parameters for bottom of F2, F1 and E
                             	 	 	 * layers respectively */
  double pdTopThick[LAYER_COUNT_SI];    /**< Thickness parameters for top of F2, F1 and E
                             	 	 	 * layers respectively */
  double dM3000;            			/**< Current M(3000)F2 */
  double pdF0[LAYER_COUNT_SI]; 			/**< Current fo (peak plasma frequency) for the
                                		 * F2, F1and E layers respectively */
} TLayerProperties;


typedef struct GeometryDataStruct
{
  TSPoint stP1;      /**< Positional information (km) at point P2 */
  TSPoint stP2;      /**< Positional information (km) at point P2 */
  TSPoint stRay;     /**< Coords of the ray perigee (only dLat,dLng,dR used) */
  TSPoint stPactual; /**< Only lat,lng,h used */
  double  dZeta;     /**< Zenith angle of point 2 seen from point 1 (deg)
                      * ie. angle from vertical to point 2. */
  double  dSinDelta; /**< sine of declination of sun */
  double  dCosDelta; /**< cosine of declination of sun */
  double  dSinSig;   /**< Sine of ray azimuth */
  double  dCosSig;   /**< Cosine of ray azimuth */
                     /* (Point 2 seen from point 1) (N over E to S) */
} TGeometryData;


typedef struct IntegrateDataStruct
{
  TNeQuickInputData *pstNeQuickInputData; /**< NeQuick input data */
  TGeometryData *pstGeom; 				  /**< Geometry Data */
  TCurrentCcir *pstCurrCCIR;  			  /**< Current data containing CCIR maps */
  double dTolerance; 					  /**< Tolerance for the integration */
  TBoolean bVert;  						  /**< Flag indicating vertical (TRUE) or slanted (FALSE) ray */
} TIntegrateData;


/**
 * @struct NeQuickDataStruct
 * @brief Data structure used to manage some variables needed by the NeQuick model
 */
typedef struct NeQuickDataStruct
{
	TModip stModip;
	TBoolean stModipValid;
	TCcir stCcir;
	TBoolean stCcirValid;
	TCurrentCcir stCurrCcir;
	TNeQuickInputData stNeQuickInputData;
	double modip;
	TModipRegion modipRegion;
	TBoolean modipValid;
} TNeQuickData;

#endif /* NEQUICK_NEQUICK_DATA_STRUCTURES_H_INCLUDED */
