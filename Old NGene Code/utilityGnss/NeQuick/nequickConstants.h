/**
 * @file    nequickConstants.h
 *
 * @author  Mario Nicola
 * @author  Gianluca Falco
 *
 * @date    09/01/2015
 *
 * @version 1.0.0
 *
 * @brief   Include constants used in the NeQuick model.
 *
 * @details Code from the reference NeQuick model
 *          (Constants.h, IonoConstants.h)
 */

#ifndef NEQUICK_CONSTANTS_H_INCLUDED
#define NEQUICK_CONSTANTS_H_INCLUDED


/* defines used in Nequick results management */
/**
 * @def NEQUICK_RESULTS_UPDATE_INTERVAL
 * @brief Number of seconds before last estimation of Iono delay needs an update.
 *        If zero, the receiver will execute Nequick model for each Galileo satellite, at each PVT computation.
 */
#define NEQUICK_RESULTS_UPDATE_INTERVAL		0.0


/* originally in Constants.h */
#ifndef FALSE
#define FALSE (0)
#endif
#ifndef TRUE
#define TRUE (1)
#endif

/** Constants for integer numbers */
#define ZERO_SI  (0)
#define ONE_SI   (1)
#define TWO_SI   (2)

/** Value of mathematical constant PI*2 */
#define TWOPI_D      (6.283185307179586476925287)

/** Value of mathematical constant PI */
#define PI_D         (TWOPI_D/2.0)

/** Value of mathematical constant PI/2 */
#define HALFPI_D     (PI_D/2.0)

/* Degrees to radians */
#define DEG_TO_RAD_D (PI_D/180.0)

/** Number of months of a year */
#define MONTHS_SI (12)

/**-------------------------------------------------------
 *  Constants related to CCIR and MODIP files.
 * ----------------------------------------------------- */

/** First dimension of parameters in MODIP files */
#define NUM_MODIP_DIM_1_SI (39)

/** Second dimension of parameters in MODIP files */
#define NUM_MODIP_DIM_2_SI (39)

/** First dimension of CCIR F2 parameters */
#define CCIR_F2_DIM_1_SI (13)

/** Second dimension of CCIR F2 parameters */
#define CCIR_F2_DIM_2_SI (76)

/** Third dimension of CCIR F2 parameters */
#define CCIR_F2_DIM_3_SI (2)

/** First dimension of CCIR M3000 parameters */
#define CCIR_M3000_DIM_1_SI (9)

/** Second dimension of CCIR M3000 parameters */
#define CCIR_M3000_DIM_2_SI (49)

/** Third dimension of CCIR M3000 parameters */
#define CCIR_M3000_DIM_3_SI (2)


/* originally in NeqConstants.h */
/* Constants declarations */
#define KMTOM_D          1000.0 /* Km to m conversion factor */
#define AVEMONTHDAYS_D     30.5 /* Average number of days in a month (days) */
#define MIDMONTHPT_D       15.0 /* Typical mid-month point (days) */
#define INT_BREAK1_D     1000.0 /* The 1st integration breakpoint (km) */
#define INT_BREAK2_D     2000.0 /* The 2nd integration breakpoint (km) */
#define UNITCONVERSION_D 1000.0 /* convert from internal to external units */

#define SOLARDECCONST_1_D   0.9856  /* Solar declination constant 1 */
#define SOLARDECCONST_2_D  18.0     /* Solar declination constant 2 */
#define SOLARDECCONST_3_D   3.289   /* Solar declination constant 3 */
#define SOLARDECCONST_4_D   0.39782 /* Solar declination constant 4 */
#define SOLARDECCONST_5_D   1.916   /* Solar declination constant 5 */
#define SOLARDECCONST_6_D   0.02    /* Solar declination constant 6 */
#define SOLARDECCONST_7_D 282.634   /* Solar declination constant 7 */

#define EARTH_RADIUS_D  6371.2                /* Earth radius (km) */

#define EPSTNORMALISATION_D 4.0 /* Epstein calculation normalisation factor */

#define VERTTOLRAYDR_D      0.1 /* Tolerance for vertical ray check (km) */

#define ARG_CLIP_THRESH_D  25.0 /* NeqCalcBottomsideNe arg cut off threshold */
#define NE_SCALING_D       1.0e11/* NeqCalcBottomsideNe scaling factor for Ne */

/* Constants used to calculate the Ne below the F2 peak */
#define BOTTOM_CORRN_ANCHOR_D 100.0 /* Bottomside correction anchor pt (km) */
#define BOTTOM_CORRN_FAC1_D    10.0 /* Bottomside correction fraction achor 1*/
#define BOTTOM_CORRN_FAC2_D     1.0 /* Bottomside correction fraction achor 2*/
#define BOTTOM_CORRN_SCALE_D   10.0 /* Bottomside correction scale */

#define CHI0_D            86.23292796211615 /*Zenith angle of sun*/
#define HI_R12_D          100.0 /* R12 value for hi CCIR map info */
#define EQUAL_R12_THRES_D 1.0e-10 /* Used to test R12 values for equality */
#define EQUAL_TIME_D      1.0e-20/* Used to test universal times for equality */

#define HALFDAYHOURS_D     12.0 /* Number of hours in a half day (hours) */
#define TITHERIDGE_CONST_D  1.4 /* Titheridge approximates F0F1 = 1.4 * F0E */
#define SMALL_F0F1_D       1.0e-6 /*Small  Freq F1 Peak Height*/
#define EPEAKHEIGHT_D     120.0 /* E layer peak electron density height (km) */
#define F0F1_THRESHOLD_D    0.5 /* F0F1 threshold triggers extended calcs */
#define MARCH_SI              3 /* Month number for March */
#define OCTOBER_SI           10 /* Month number for October */
#define NUM_INTER_PTS_SI      4 /* Number of points used in NeqInterpolate */
#define NUM_LNGS_SI          36 /* Number of unique longitudes in Modip map */
#define MAX_HARMONICS_SI      6 /* Max harmonics in NeqCalcSphLegCoeffs */
#define NUM_F0_HARMONICS_SI   6 /* Num F0 harmonics in NeqCalcSphLegCoeffs */
#define NUM_M3K_HARMONICS_SI  4 /* Num M3000 harmonics in NeqCalcSphLegCoeffs */
#define MAX_CCIRROWS_SI      76 /* Max num of rows in a CCIR map */
#define MAX_SINMODIP_TERM_SI 12 /* Max num of sin(Modip)^N terms in
                                 * NeqGetF2FreqFromCCIR */
#define SINMODIP_CUTOFF_D 1.0e-30 /* NeqGetF2FreqFromCCIR sin(Modip)^N cutoff */
#define FREQ2NE_FACTOR_D    0.124 /* Conversion factor in NeqCriticalFreqToNe */

#define F0F1_JOINFAC1_D  1000.0   /*Freq F1 peak height join fractional */
#define F0F1_JOINFAC2_D    60.0   /*Freq F1 peak height join fractional */
#define F0F1_CONST1_D       0.85  /*Freq F1 peak height constant*/

#define THICK_SCALING_F2B_D 0.385 /*Scaling F2 bottomside thickness*/
#define THICK_SCALING_F1T_D 0.3   /*Scaling F1 topside thickness*/
#define THICK_SCALING_F1B_D 0.5   /*Scaling F2 bottomside thickness*/
#define MIN_ET_THICK_D      7.0 /* Minimum thickness for E layer topside (km) */
#define EB_THICK_D          5.0 /* Thickness for E layer bottomside (km) */

#define NUM_AMP_ITER_SI       5 /* Number of iterations in extended calcs */
#define F1_AMP_CONST_D      0.8 /* Const for calculating F1 amplitude */
#define E_AMP_CONST1_D     5.0e-2 /* Const for calculating E amplitude */
#define E_AMP_JOINFAC_D    60.0 /* Const for calculating E amplitude */
#define E_AMP_CONST2_D     5.0e-3 /* Const for calculating E amplitude */

/* Constants for conversion of Az to R12 sunspot number */
#define AZTOSSN_1_D   63.7
#define AZTOSSN_2_D 1123.6
#define AZTOSSN_3_D 167273.0
#define AZTOSSN_4_D   408.99

/* Constants for calculating FoE */
#define F0E_CONST1_D        0.3
#define F0E_CONST2_D       90.0
#define F0E_CONST3_D        0.24
#define F0E_CONST4_D       20.0
#define F0E_CONST5_D        0.2
#define F0E_CONST6_D       12.0
#define F0E_CONST7_D        1.112
#define F0E_CONST8_D        0.019
#define F0E_CONST9_D        0.3
#define F0E_CONST10_D       0.49

/* Constants used to calculate density gradient at F2 base */
#define DENSE_GRAD1_D       0.01
#define DENSE_GRAD2_D      -3.467
#define DENSE_GRAD3_D       0.857
#define DENSE_GRAD4_D       2.02

/* Constants used to calculate density gradient at F2 base */
#define SHAPE_FACTOR_1_D    6.705/*Constant 1 for density gradient at F2 base */
#define SHAPE_FACTOR_2_D    0.014/*Constant 2 for density gradient at F2 base */
#define SHAPE_FACTOR_3_D    0.008/*Constant 3 for density gradient at F2 base */
#define SHAPE_FACTOR_4_D   -7.77 /*Constant 4 for density gradient at F2 base */
#define SHAPE_FACTOR_5_D    0.097/*Constant 5 for density gradient at F2 base */
#define SHAPE_FACTOR_6_D    0.153/*Constant 6 for density gradient at F2 base */
#define SHAPE_FACTOR_7_D    2.0  /*Constant 7 for density gradient at F2 base */
#define SHAPE_FACTOR_8_D    8.0  /*Constant 8 for density gradient at F2 base */

/* Constants used to calculate the F2 topside thickness */
#define TOP_CORRN_CONST1_D 150.0      /*F2 Topside thickness constant 1 */
#define TOP_CORRN_CONST2_D 100.0      /*F2 Topside thickness constant 2 */
#define TOP_CORRN_CONST3_D  0.041163  /*F2 Topside thickness constant 3 */
#define TOP_CORRN_CONST4_D  0.183981  /*F2 Topside thickness constant 4 */
#define TOP_CORRN_CONST5_D  1.424472  /*F2 Topside thickness constant 5 */

/* Constants used in interpolation routine */
#define INTER_THRESH_D    5.0e-11 /* Threshold above which interpolation done */

#define INTER_COEFF1_SI       0 /* Coefficient 1 in interpolation */
#define INTER_COEFF2_SI       1 /* Coefficient 2 in interpolation */
#define INTER_COEFF3_SI       2 /* Coefficient 3 in interpolation */
#define INTER_COEFF4_SI       3 /* Coefficient 4 in interpolation */
#define INTERPOL_CONST1_D   9.0 /* Constant used in NeqInterpolate */
#define INTERPOL_CONST2_D  16.0 /* Constant used in NeqInterpolate */

/* Constants for the spherical Legendre coefficients for F0F2 */
#define NUM_QF0_SI            9 /* Num of spherical Legendre coeffs for F0F2 */
#define QF0_1_SI             11 /* Spherical Legendre coefficient 1 for F0F2 */
#define QF0_2_SI             11 /* Spherical Legendre coefficient 2 for F0F2 */
#define QF0_3_SI              8 /* Spherical Legendre coefficient 3 for F0F2 */
#define QF0_4_SI              4 /* Spherical Legendre coefficient 4 for F0F2 */
#define QF0_5_SI              1 /* Spherical Legendre coefficient 5 for F0F2 */
#define QF0_6_SI              0 /* Spherical Legendre coefficient 6 for F0F2 */
#define QF0_7_SI              0 /* Spherical Legendre coefficient 7 for F0F2 */
#define QF0_8_SI              0 /* Spherical Legendre coefficient 8 for F0F2 */
#define QF0_9_SI              0 /* Spherical Legendre coefficient 9 for F0F2 */

/* Constants for the spherical Legendre coefficients for M3000 */
#define NUM_QM3000_SI         7 /* Num of spherical Legendre coeffs for M3000 */
#define QM3000_1_SI           6 /* Spherical Legendre coefficient 1 for M3000 */
#define QM3000_2_SI           7 /* Spherical Legendre coefficient 2 for M3000 */
#define QM3000_3_SI           5 /* Spherical Legendre coefficient 3 for M3000 */
#define QM3000_4_SI           2 /* Spherical Legendre coefficient 4 for M3000 */
#define QM3000_5_SI           1 /* Spherical Legendre coefficient 5 for M3000 */
#define QM3000_6_SI           0 /* Spherical Legendre coefficient 6 for M3000 */
#define QM3000_7_SI           0 /* Spherical Legendre coefficient 7 for M3000 */

/* Constants used in Dudeney calculations for F2 Peak Height */
#define DUDENEY_CONST1_D    0.0196  /*Dudeney Constant 1*/
#define DUDENEY_CONST2_D    1.2967  /*Dudeney Constant 2*/
#define DUDENEY_CONST3_D    0.012   /*Dudeney Constant 3*/
#define DUDENEY_CONST4_D    0.253   /*Dudeney Constant 4*/
#define DUDENEY_CONST5_D    1.215   /*Dudeney Constant 5*/
#define DUDENEY_CONST6_D 1490.0     /*Dudeney Constant 6*/
#define DUDENEY_CONST7_D  176.0     /*Dudeney Constant 7*/
#define DUDENEY_JOIN_D     20.0     /*Dudeney Join Constant */
#define DUDENEY_CLIP_D      1.75    /*Dudeney Clip Constant */
#define DUDENEY_SMALL_F0E_D 1.0e-30 /*Dudeney small FOE Constant */

#define TOPSIDE_CORRN1_D    0.125 /* Constant used in NeCalcTopsideNe calcs */
#define TOPSIDE_CORRN2_D  100.0 /* Constant used in NeCalcTopsideNe calcs */
#define LARGE_EE_D         1.0e11/* Cut off for NeqCalcTopsideNe approximation*/
#define UNSET_NMAX_TOL_D   1.0e-10  /* Tolerance for checking Nmax is unset */

#define VERTTOLLATLNG_D    1.0e-5 /* Tolerance for vertical ray check (deg) */
#define MAX_ZETA_D         90.0 /* Max value of zeta (zenith angle) */
#define RAYPOLETOL_D      1.0e-10 /* Tolerance for checking ray not at pole */

#define MAX_DETAILS_LEN_SI 1024 /* Maximum length of addLogMessage details */
#define KRONROD_K15_PTS_SI   15 /* Num of K15 pts for Kronrod integration */
#define KRONROD_G7_PTS_SI     7 /* Num of G7 pts for Kronrod integration */

/* Constants for where samples are taken in the Kronrod integration (i.e. how
 * far from the midpoint). */
#define KRONROD_POINT_1_D   0.207784955007898467600689403773245/* INT Point 1 */
#define KRONROD_POINT_2_D   0.405845151377397166906606412076961/* INT Point 2 */
#define KRONROD_POINT_3_D   0.586087235467691130294144838258730/* INT Point 3 */
#define KRONROD_POINT_4_D   0.741531185599394439863864773280788/* INT Point 4 */
#define KRONROD_POINT_5_D   0.864864423359769072789712788640926/* INT Point 5 */
#define KRONROD_POINT_6_D   0.949107912342758524526189684047851/* INT Point 6 */
#define KRONROD_POINT_7_D   0.991455371120812639206854697526329/* INT Point 7 */

/* Constants for weights for K15 sample points in Kronrod integration */
#define KRONROD_K15_1_D     0.022935322010529224963732008058970/* Weight 1*/
#define KRONROD_K15_2_D     0.063092092629978553290700663189204/* Weight 2*/
#define KRONROD_K15_3_D     0.104790010322250183839876322541518/* Weight 3*/
#define KRONROD_K15_4_D     0.140653259715525918745189590510238/* Weight 4*/
#define KRONROD_K15_5_D     0.169004726639267902826583426598550/* Weight 5*/
#define KRONROD_K15_6_D     0.190350578064785409913256402421014/* Weight 6*/
#define KRONROD_K15_7_D     0.204432940075298892414161999234649/* Weight 7*/
#define KRONROD_K15_8_D     0.209482141084727828012999174891714/* Weight 8*/

/* Constants for weights for G7 sample points in Kronrod integration */
#define KRONROD_G7_1_D      0.129484966168869693270611432679082/* Weight 1*/
#define KRONROD_G7_2_D      0.279705391489276667901467771423780/* Weight 2*/
#define KRONROD_G7_3_D      0.381830050505118944950369775488975/* Weight 3*/
#define KRONROD_G7_4_D      0.417959183673469387755102040816327/* Weight 4*/

#define CLIPEXP_CUTOFF_D   80.0     /* Cut off value for NeqClipExp calcs */
#define CLIPEXP_HIGH_D   5.5406e34  /* High value for NeqClipExp ie. exp(>80) */
#define CLIPEXP_LOW_D    1.8049e-35 /* Low value for NeqClipExp ie. exp(<-80) */

#define NUM_CCIR_F2_PTS_SI    (CCIR_F2_DIM_1_SI * CCIR_F2_DIM_2_SI)
                            /* Number of values in a CCIR F2 map */
#define NUM_CCIR_M3000_PTS_SI (CCIR_M3000_DIM_1_SI * CCIR_M3000_DIM_2_SI)
                            /* Number of values in a CCIR M3000 map */

/* Epstein layer indices (and number of layers) */
#define LAYER_F2_SI 		(0) /*Layer at F2 Peak Height */
#define LAYER_F1_SI 		(1) /*Layer at F1 Peak Height */
#define LAYER_E_SI 		  (2) /*Layer at Epstein */
#define LAYER_COUNT_SI  (3) /*Number of Epstein Layer */

/*	for deciding which freq NeqGetF2FreqFromCCIR is to calculate */
#define CCIR_MODE_F0_SI 		(0)/*F0 frequency to be calculated */
#define CCIR_MODE_M3000_SI (1) /*M3000 frequency to be calculated */




/* originally in IonoConstants.h */

/** Constants for floating point numbers */
#define TWO_D (2.0)
#define THREE_D (3.0)
#define NINE_D (9.0)
#define SIXTEEN_D (16.0)

#define MAX_PERCENTAGE_D (100.0) /* Value of 100 procent */

#define CIRCLEDEG_D (360.0)                   /* Degrees in a cirle */
#define HALFCIRCLEDEG_D (CIRCLEDEG_D / TWO_D) /* Degrees in half a circle */
#define TWOCIRCLEDEG_D (CIRCLEDEG_D * TWO_D)  /* Degreeds in two circles */

#define DEGPERHOUR_D (15.0) /* Number of degrees per hour (deg) */
#define SAFE_GAMMA_D (10.0) /* Save gamma value */


#define CONVSTEC_D (40.3) /* Value for convertion of delay in metres to TEC  */

#define MAX_OBSSTEC_EPOCHS_SI (300) /* Maximum number of Epoch Observations*/
#define MAX_OBSSTEC_OBS_SI (MAX_OBSSTEC_EPOCHS_SI * MAX_SIV_ONE_GSS_SI)
                                /* Maximum number of Observations*/

#define COORD_EAST_SI (0)       /* East-coordinate value */
#define COORD_NORTH_SI (1)      /* North-coordinate value */
#define COORD_UP_SI (2)         /* Up-coordinate value */
#define NUMCOORDS_ENU_SI (3)    /* Total number of coordinates */

#define NUMCOORDS_XYZ_SI (3)    /* Total number of coordinates */

#define COORD_LAT_SI (0)        /* Latitude value */
#define COORD_LNG_SI (1)        /* Longitude value */
#define COORD_H_SI (2)          /* Height value */
#define NUMCOORDS_LLH_SI (3)    /* Total number of coordinates */

#define WEEKDAYS_SI (7)         /* Total number of weekdays */
#define DAYHOURS_SI (24)        /* Integer number of hours in a day */
#define DAYHOURS_D (24.0)       /* Floating point number of hours in a day */

#define SEMAXE_D  (6378136.6)   /* Semi-major axis of Earth (m) */
#define FLATTE_D  (1.0 / 298.25642)  /* Earths Flattening factor */
#define CUBEPOWER_D (3.0)       /* Raise to power 3 i.e. cubed */

#define GAL_START_DAY_SI  (234)   /* Galileo start time day of year*/
#define GAL_START_YEAR_SI (1999)  /* Galileo start time year */

#define DAYS_IN_NONLEAPYEAR_SI (365)  /* Number of days in non-leap year */

#define YEARS_1_SI   (1) /* 1 year */
#define YEARS_4_SI   (4) /* 4 years */
#define YEARS_100_SI (100) /* 100 years */
#define YEARS_400_SI (400) /* 400 years */

/* Number of days in each month of non-leap year */
#define NUMDAYS_JAN_SI (31) /* Number of days in January */
#define NUMDAYS_FEB_SI (28) /* Number of days in Febuary (non leap year) */
#define NUMDAYS_MAR_SI (31) /* Number of days in March */
#define NUMDAYS_APR_SI (30) /* Number of days in April */
#define NUMDAYS_MAY_SI (31) /* Number of days in May */
#define NUMDAYS_JUN_SI (30) /* Number of days in June */
#define NUMDAYS_JUL_SI (31) /* Number of days in July */
#define NUMDAYS_AUG_SI (31) /* Number of days in August */
#define NUMDAYS_SEP_SI (30) /* Number of days in September */
#define NUMDAYS_OCT_SI (31) /* Number of days in October */
#define NUMDAYS_NOV_SI (30) /* Number of days in November */
#define NUMDAYS_DEC_SI (31) /* Number of days in December */

#define LEAPYEAR_EVERY_SI  (YEARS_4_SI)   /* Nominal Frequency of leap-years */
#define LEAPYEAR_EXCEPT_SI (YEARS_100_SI) /* Exception leap year rule (multiple
                                           * 100 ) */
#define LEAPYEAR_ALSO_SI   (YEARS_400_SI) /* Include multiple of 400 in leap
                                           * year */

#define RANGE_LATDEG_MIN_D (-90.0)  /* Minimum latitude in degrees */
#define RANGE_LATDEG_MAX_D  (90.0)  /* Maximum latitude in degrees */
#define RANGE_LONDEG_MIN_D (-180.0) /* Minimum longitude in degrees */
#define RANGE_LONDEG_MAX_D  (180.0) /* Maximum longitude in degrees */
#define MODIP_DIFF_THRES_D   (0.1)  /* Modip Difference Threshold */

#define MODIP_LAT_95N_SI       (0) /* Modip value at Latitude 95 deg N */
#define MODIP_LAT_90N_SI       (1) /* Modip value at Latitude 90 deg N */
#define MODIP_LAT_85N_SI       (2) /* Modip value at Latitude 85 deg N */
#define MODIP_LAT_85S_SI      (36) /* Modip value at Latitude 85 deg S */
#define MODIP_LAT_90S_SI      (37) /* Modip value at Latitude 90 deg S */
#define MODIP_LAT_95S_SI      (38) /* Modip value at Latitude 95 deg S */

#define MODIP_LNG_190W_SI      (0) /* Modip value at Longitude 190 deg W*/
#define MODIP_LNG_180W_SI      (1) /* Modip value at Longitude 180 deg W*/
#define MODIP_LNG_170W_SI      (2) /* Modip value at Longitude 170 deg W*/
#define MODIP_LNG_0_SI        (19) /* Modip value at Longitude 0 deg */
#define MODIP_LNG_170E_SI     (36) /* Modip value at Longitude 170 deg E*/
#define MODIP_LNG_180E_SI     (37) /* Modip value at Longitude 180 deg E*/
#define MODIP_LNG_190E_SI     (38) /* Modip value at Longitude 190 deg E*/

#define MODIP_LNG_180DEG_SI   (18) /* Modip value at Longitude 180 */
#define MODIP_LNG_360DEG_SI   (36) /* Modip value at Longitude 360 */

#define RADTODEG_D (180.0 / PI_D) /* Value of constant required to convert */
                                  /* from radians to degrees */
#define LON_STEP_SI (10)  /* Size of longitude step in Modip grid */
#define NUM_LONS_SI (36)  /* Number of unique longitudes in Modip map */
#define LAT_STEP_SI (5)   /* Size of latitude step in Modip grid */

#define INT_PT0_SI (0)        /* Interpolation point 0 */
#define INT_PT1_SI (1)        /* Interpolation point 1 */
#define INT_PT2_SI (2)        /* Interpolation point 2 */
#define INT_PT3_SI (3)        /* Interpolation point 3 */
#define NUM_INT_PTS_SI (4)    /* Number of points used for interpolation */

#define MIN_DELTAX_D (1e-10)  /* Minimum value for delta X */

#define TECU_D (1.0e+16)      /* Total Electron Content Unit */



#endif /* NEQUICK_CONSTANTS_H_INCLUDED */
