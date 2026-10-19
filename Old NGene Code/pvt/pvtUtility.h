/**
 * @file pvtUtility.h
 *
 * @author Mario Nicola
 *
 * @date 23/10/2014
 *
 * @version 1.0.0
 *
 * @brief Some utility function for the PVT computation.
 */

#ifndef PVT_UTILITY_H_INCLUDED
#define PVT_UTILITY_H_INCLUDED

#include "navigationMessageDataStructure.h"
#include "spreadingCodes.h"

/**
 * @brief Keep into account for beginning or end of week crossover.
 *
 * @param t1, t2 are time in sec
 * @return corrTime is the time corrected
 */
double CheckT(double t1,double t2);

/*
 * @brief Calculates azimuth and elevation w.r.t. the last computed receiver position
 *
 * @param svPos is the satellite position
 * @param solPos is the receiver position
 * @param azimuth is the output [rad]
 * @param elevation is the output [rad]
 */
void ComputeAzEl(double *svPos,double *solPos,double *azimuth,double *elevation);

/*
 * @brief Calculates position, velocity, azimuth and elevation w.r.t. the receiver position
 *        for one satellite using the almanac (read from file or received through navigation message)
 *
 * @param satType is the type of satellite
 * @param navMesg points to the content of the navigation message (almanac included) for the current PRN
 * @oaram tow is the Time Of Week [s]
 * @param ecefPosition Receiver position (X,Y,Z) in ECEF frame [m]
 * @param ecefSatPos Output, Satellite position (X,Y,Z) in ECEF frame [m]
 * @param ecefSatVel Output, satellite velocity (Vx,Vy,Vz) in ECEF frame [m]
 * @param azimuth Output, is the azimuth of the satellite w.r.t. the receiver [rad]
 * @param elevation Output, is the elevation of the satellite w.r.t. the receiver  [rad]
 */
void ComputeSatPositionFromAlmanac(TSatConstellation satType,TNavigationMessage *navMesg,double tow,double *ecefPosition,double *ecefSatPos,double *ecefSatVel,double *azimuth,double *elevation);

/*
 * @brief Calculates ionospheric delay correction for GPS
 *
 * @param ionosphere points to the ionosphere correction parameters for the current satellite
 * @param elevation is the elevation value
 * @param azimuth is the azimuth value
 * @param latu is the user latitude
 * @param lonu is the user longitude
 * @param transmissionTime is the transmission time
 * @return Ionosphere delay [s]
 */
double GpsIonosphericCorrection(TGnssIono *ionosphere, double elevation, double azimuth, double latu, double lonu, double transmissionTime);

/*
 * @brief Calculates troposphere delay correction
 *
 * @param elevation is the elevation of the current satellite
 * @param h is the altitude of the receiver (geodetic reference system)
 * @return Troposphere delay [m]
 */
double TropoHopfieldModel(double elevation, double h);

/**
 * @brief This function calculate the troposphere correction.
 *
 * @param sinel is the sin of elevation angle of satellite
 * @param hsta is the height of station in km
 * @param p is the atmospheric pressure in mb at height h
 * @param tkel is the surface temperature in degrees Kelvin at height htkel
 * @param hum is the humidity in % at height hhum
 * @param hp is the height of pressure measurement in km
 * @param htkel is the height of temperature measurement in km
 * @param hhum is the height of humidity measurement in km
 * @return Troposphere delay [m]
 */
double Tropo(double sinel, double hsta, double p, double tkel, double hum, double hp, double htkel, double hhum);

/**
 * @brief Corrects the sat position due to Earth rotation
 *
 * @param travelTime is the estimated travel time
 * @param x is the input
 * @param rotX is the output
 * @param numOfSat number of satellites used in PVT
 * @param channel index of the channel allocated to the satellite under elaboration
 */
void EarthCorrection(double travelTime, double *x, double *rotX, int numOfSat, int channel);

/**
 * @brief Transform vector dx (user - sat) into topocentric coordinate system with origin at X.
 *
 * @param ecefCoord is the user coordinates (in ECEF system [X; Y; Z;])
 * @param satEcef is the sat coordinates
 * @param satAngle is the return value containing elevation, azimuth and distance of the vector (user - sat) (degrees)
 */
void Topocent(double *ecefCoord, double *satEcef, double *satAngle);

/**
 * @brief Convert the geographical coordinates (lon, lat, h) on the WSG84 reference ellipsoid to Cartesian coordinates (X,Y,Z).
 *
 * @param geodCoord is the return value; it's the array containing the longitude (degree), latitude (degree), height (m)
 * @param ecefCoord is the array containing the coordinates in ECEF
 */
void GeodToEcef(double *geodCoord,double *ecefCoord);

/**
 * @brief Convert the Cartesian coordinates (X,Y,Z) to geographical coordinates (lon, lat, h) on the WSG84 reference ellipsoid.
 *
 * @param ecefCoord is the array containing the coordinates in ECEF
 * @param geodCoord is the return value; it's the array containing the longitude (degree), latitude (degree), height (m)
 */
void Ecef2Geod(double *ecefCoord, double *geodCoord);

/**
 * @brief This function convert LLA coordinates in ENU coordinates (Intecs)
 *
 * @param lat is the latitude (rad)
 * @param lon is the longitude (rad)
 * @param alt is the altitude (m)
 * @param enu is the array containing the E,N,U coordinates (returned value)
 * @param utmZone is the UTM zone
 */
void LlaToUtm(double lat, double lon, double alt, double *enu, double *utmZone);


/**
 * @brief This function convert user velocity from ECEF to ENU (Intecs)
 *
 * @param ecefPos is an array containing the user coordinates in ECEF reference system
 * @param ecefVel is an array containing the user velocities in ECEF reference system (m/s)
 * @param enuPos is the array containing the user position in ENU reference system
 * @param enuVel is the array containing the user velocities in ENU (m/s)
 */
void EcefVelToEnuVel(double *ecefPos,double *ecefVel,double *enuPos,double *enuVel);


/**
 * @brief This function evaluates the geoid separation for the current position.
 *        From ASSIST2.
 *
 * @param lat is the latitude in degrees
 * @param lon is the longitude in degrees
 *
 * @return geoid separation [m]
 */
double GetWgs84Separation(double lat,double lon);


#endif /* PVT_UTILITY_H_INCLUDED */
