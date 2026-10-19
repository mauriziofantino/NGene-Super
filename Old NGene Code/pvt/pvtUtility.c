/**
 * @file pvtUtility.c
 *
 * @author Mario Nicola
 *
 * @date 23/10/2014
 *
 * @version 1.0.0
 *
 * @brief Some utility function for the PVT computation.
 */

#include <math.h>
#include <stdlib.h>
#include "pvtUtility.h"
#include "ngeneConstants.h"
#include "pvtConstants.h"
#include "navigationMessageDataStructure.h"
#include "matrices.h"
#include "spreadingCodes.h"


double CheckT(double t1, double t2)
{
	double time;
	double half_week = 302400;
	double corrTime;

	time = t1 - t2;
	corrTime = time;

	if(time > half_week)
	{
		corrTime = time - 2*half_week;
	}
	else if(time < -half_week)
	{
		corrTime = time + 2*half_week;
	}
	return corrTime;
}


void ComputeAzEl(double *svPos,double *solPos,double *azimuth,double *elevation)
{
	double p = 0; /* user distance from earth center projected on the equatorial plane */
	double r = 0; /* user distance from earth center */
	double s = 0;

	double e[3*3];
	MatrixInit(e,3,3);
	double d[1*3];
	MatrixInit(d,1,3);

	p = sqrt(pow(solPos[0], 2) + pow(solPos[1], 2));
	r = sqrt(pow(solPos[0], 2) + pow(solPos[1], 2) + pow(solPos[2], 2));

	if((r == 0) || (p == 0))
	{
		return;
	}

	e[0] = -1.0 * (solPos[1] / p);
	e[1] = +1.0 * (solPos[0] / p);
	e[2] = 0.0;
	e[3] = -1.0 * (solPos[0] * solPos[2] / (p * r));
	e[4] = -1.0 * (solPos[1] * solPos[2] / (p * r));
	e[5] = p / r;
	e[6] = solPos[0] / r;
	e[7] = solPos[1] / r;
	e[8] = solPos[2] / r;

	int k;
	int t;
	for(k = 0; k < 3; k++)
	{
		for(t = 0; t < 3; t++)
		{
			d[k] = d[k] + (svPos[t] - solPos[t]) * e[t+(3*k)];
		}
	}

	s = d[2] / sqrt(pow(d[0], 2) + pow(d[1], 2) + pow(d[2], 2));

	if(s == 1)
	{
	    *elevation = 0.5 * M_PI;
	}
	else
	{
	    *elevation = atan(s / sqrt(1 - (s * s)));
	}

	if((d[1] == 0) && (d[0] > 0))
	{
	    *azimuth = 0.5 * M_PI;
	}
	else
	{
	    if((d[1] == 0) && (d[0] < 0))
	    {
	        *azimuth = 1.5 * M_PI;
	    }
	    else
	    {
	    	*azimuth = atan(d[0] / d[1]);
	    	if(d[1] < 0.0)
	    	{
	    		*azimuth = *azimuth + M_PI;
	    	}
	    	else
	    	{
	    		if((d[1] > 0) && (d[0] < 0))
	    		{
	    			*azimuth = *azimuth + (2 * M_PI);
	    		}
	    	}
	    }
	}
}


void ComputeSatPositionFromAlmanac(TSatConstellation satType,TNavigationMessage *navMesg,double tow,double *ecefPosition,double *ecefSatPos,double *ecefSatVel,double *azimuth,double *elevation)
{
	TGnssAlmanac *alm = &(navMesg->almanac);
	double dt, time, satClkCorr;
	double a, tk, n0, n, m, e, ii, eOld, dE, nu, phi, u, r, i, omega;
	double tak, xpk, ypk, takdot, ekdot, ukdot, rkdot, ikdot, xpkdot, ypkdot, omegak, omegakdot;
	double xkdot, ykdot, zkdot;
	// fake variables
	double deltaN = 0.0;
	double capitalCuc = 0.0;
	double capitalCus = 0.0;
	double capitalCrc = 0.0;
	double capitalCrs = 0.0;
	double iDot = 0.0;
	double capitalCic = 0.0;
	double capitalCis = 0.0;

	/* check availability of almanac */
	if((satType==GPS_SAT) && (navMesg->almanac1Received==FALSE_FLAG))
		return;
	if((satType==GALILEO_SAT) && ((navMesg->almanac1Received==FALSE_FLAG) || (navMesg->almanac2Received==FALSE_FLAG)))
		return;

	/* transmitted clock corrections */
	dt = CheckT(tow,alm->toa);
	satClkCorr = (alm->af[1] * dt) + alm->af[0];
	/* correct the transmission time */
	time = tow - satClkCorr;

	/* compute sat position */
	a = alm->sqrtA * alm->sqrtA;
	tk = CheckT(time, alm->toa);
	n0 = sqrt(GM / (a * a * a));
	n = n0 + deltaN;
	m = alm->meanAnomaly0 + n * tk;
	m = fmod(m + TWICE_M_PI,TWICE_M_PI);
	e = m;
	for(ii=0;ii<10;ii++)
	{
		eOld = e;
		e = m + alm->eccentricity * sin(e);
		dE = fmod(e-eOld,TWICE_M_PI);
		if(fabs(dE)<1E-12)
		{
			// Necessary precision is reached, exit from the loop
			break;
		}
	}
	e = fmod(e + TWICE_M_PI, TWICE_M_PI);
	nu = atan2(sqrt(1 - alm->eccentricity *	alm->eccentricity) * sin(e), cos(e)- alm->eccentricity);
	phi = nu + alm->omega;
	phi = fmod(phi,TWICE_M_PI);
	u = phi + capitalCuc * cos(2*phi) + capitalCus * sin(2*phi);
	r = a * (1 - alm->eccentricity * cos(e)) + capitalCrc * cos(2*phi) + capitalCrs * sin(2*phi);
	i = alm->i0 + iDot * tk + capitalCic * cos(2*phi) + capitalCis * sin(2*phi);
	omega = alm->capitalOmega0 + (alm->capitalOmegaDot - OMEGAE_DOT) * tk -	OMEGAE_DOT * alm->toa;
	omega = fmod(omega + TWICE_M_PI, TWICE_M_PI);
	// output
	ecefSatPos[0] = (cos(u) * r * cos(omega)) - (sin(u) * r * cos(i) * sin(omega));
	ecefSatPos[1] = (cos(u) * r * sin(omega)) + (sin(u) * r * cos(i) * cos(omega));
	ecefSatPos[2] = (sin(u) * r * sin(i));

	/* compute sat velocity */
	ekdot = n/(1.0 - alm->eccentricity * cos(e));
	tak = atan2( sqrt(1.0 - alm->eccentricity * alm->eccentricity) * sin(e), cos(e) - alm->eccentricity);
	takdot = sin(e) * ekdot * (1.0 + alm->eccentricity *cos(tak)) / (sin(tak) * (1.0 - alm->eccentricity * cos(e)));
	ukdot = takdot + 2.0 *((capitalCus * cos(2.0 * u)) - (capitalCuc * sin(2.0 * u))) * takdot;
	rkdot = ((a * alm->eccentricity * sin(e) * n) / (1.0 - alm->eccentricity * cos(e))) + (2.0 * (capitalCrs * cos(2.0 * u) - capitalCrc * sin(2.0 * u)) * takdot);
	ikdot = iDot + ((capitalCis *	cos(2.0 * u)) - (capitalCic * sin(2.0 * u))) * 2.0 * takdot;
	xpk = r*cos(u);
	ypk = r*sin(u);
	xpkdot = rkdot * cos(u) - ypk * ukdot;
	ypkdot = rkdot * sin(u) + xpk * ukdot;
	omegak = omega;
	omegakdot = (alm->capitalOmegaDot - OMEGAE_DOT);
	xkdot = (xpkdot - ypk * cos(i) * omegakdot) * cos(omegak) - (xpk * omegakdot + ypkdot * cos(i) - ypk * sin(i) * ikdot) * sin(omegak);
	ykdot = (xpkdot - ypk * cos(i) * omegakdot) * sin(omegak) + (xpk * omegakdot + ypkdot * cos(i) - ypk * sin(i) * ikdot) * cos(omegak);
	zkdot = ypkdot * sin(i) + ypk * cos(i) * ikdot;
	// output
	ecefSatVel[0] = xkdot;
	ecefSatVel[1] = ykdot;
	ecefSatVel[2] = zkdot;

	/* azimuth and elevation */
	ComputeAzEl(ecefSatPos,ecefPosition,azimuth,elevation);
}


double GpsIonosphericCorrection(TGnssIono *ionosphere, double elevation, double azimuth, double latu, double lonu, double transmissionTime)
{
	double phi;		// Earth-centered angle
	double lati;	// Subionospheric latitude
	double loni;	// Subionospheric latitude
	double latm;	// Geom. latitude of the iono inters. point
	double t;		// Local time at subionospheric point
	double f;		// Slant Factor
	double x;		// Radiants
	double per;
	double amp;
	double dTiono;	// Ionospheric delay correction

	double azimuthRad = azimuth;

	//{convert from radians to semicircles}
	//Latu := Latu / pi; Lonu := Lonu / pi; Az := Az / pi; El := El / pi;
	latu = latu / M_PI;
	lonu = lonu / M_PI;
	elevation = elevation / M_PI;
	azimuth = azimuth / M_PI;

	//	{calculation}
	//	phi := 0.0137 / (El + 0.11) - 0.022;
	//	Lati := Latu + phi * cos (Az * pi);
	//	if Lati > +0.416 then Lati := +0.416 else if Lati < -0.416 then Lati := -0.416;
	//	Loni := Lonu + phi * sin(Az * pi) / cos(Lati * pi);
	//	Latm := Lati + 0.064 * cos((Loni - 1.617) * pi);
	//	T := 4.32E+4 * Loni + Ttr;
	//	while T >= 86400 do T := T - 86400 else while T < 0 do T := T + 86400;
	phi = 0.0137 / (elevation + 0.11) - 0.022;
	lati = latu + phi * cos(azimuthRad);
	if(lati > 0.416)
	{
		lati = 0.416;
	}
	else if(lati < -0.416)
	{
		lati = -0.416;
	}
	loni = lonu + phi * sin(azimuthRad) / cos(lati*M_PI);
	latm = lati + 0.064 * cos((loni - 1.617) * M_PI);
	t = 4.32E+4 * loni + transmissionTime;
	if(t >= 86400)
	{
		while(t >= 86400)
		{
			t = t - 86400;
		}
	}
	else if(t < 0)
	{
		while(t < 0)
		{
			t = t + 86400;
		}
	}

	//	F := 1.0 + 16.0 * (0.53 - El) * (0.53 - El) * (0.53 - El);
	//	per := b0 + b1 * Latm + B2 * Latm * Latm + b3 * Latm * Latm * Latm;
	//	if per < 72000.0 then per := 72000.0;
	//	x := 2 * pi * (T - 50400.0) / per;
	//	amp := a0 + a1 * Latm + A2 * Latm * Latm + a3 * Latm * Latm * Latm;
	//	if amp < 0.0 then amp := 0.0;
	//	if abs(x) >= 1.57 then dTiono := F * 5.0E-9 else
	//	   dTiono := F * (5.0E-9 + amp * (1.0 - x * x / 2.0 + x * x * x * x /24.0))

	f = 1.0 + 16.0 * pow((0.53 - elevation), 3.0);
	per =  ionosphere->beta[0] +
		  (ionosphere->beta[1] * latm) +
          (ionosphere->beta[2] * pow(latm, 2.0)) +
          (ionosphere->beta[3] * pow(latm, 3.0));
	if(per < 72000.0)
	{
		per = 72000.0;
	}
	x = 2 * M_PI * (t - 50400.0) / per;
	amp =  ionosphere->alphaGps[0] +
	      (ionosphere->alphaGps[1] * latm) +
		  (ionosphere->alphaGps[2] * pow(latm, 2.0)) +
		  (ionosphere->alphaGps[3] * pow(latm, 3.0));
	if(amp < 0.0)
	{
		amp = 0.0;
	}
	if(fabs(x) >= 1.57)
	{
		dTiono = f * 5.0E-9;
	}
	else
	{
		dTiono = f * (5.0E-9 + amp * (1.0 - (pow(x, 2)/2.0) + (pow(x, 4)/24.0)));
	}
	return dTiono;
}


double TropoHopfieldModel(double elevation, double h)
{
	double tropoDelay;
	double num = 1e-6 * K1 * RD * P0_45;
	double den = GM1;
	double zhdy = num / den;
	num = 1e-6 * K2 * RD;
	den = (GM1 * (LAMBDA0_45 + 1)) - (BETA0_45 * RD);
	double zwet = (num/den) * (E0_45 / T0_45);
	num = 1 - ((BETA0_45 * h) / T0_45);
	double exp1 = GRAVITY_ACCELERATION / (RD * BETA0_45);
	double exp2 = (((LAMBDA0_45 + 1)* GRAVITY_ACCELERATION)/(RD * BETA0_45)) - 1;
	if(num <= 0)
	{
	    return 0.0;
	}
	double dhdy = zhdy * pow(num, exp1);
	double dwet = zwet * pow(num, exp2);
	double mEl = 1.001/sqrt( (0.002001 + pow(sin(elevation),2)));
	tropoDelay = -1.0 * (dhdy + dwet) * mEl;

	return tropoDelay;
}

double Tropo(double sinel, double hsta, double p, double tkel, double hum, double hp, double htkel, double hhum)
{
	double a_e    = 6378.137;
	double b0     = 7.839257e-5;
	double tlapse = -6.5;
	double tkhum  = tkel + tlapse * (hhum - htkel);
	double atkel  = 7.5 * (tkhum-273.15) / (237.3 + tkhum - 273.15);
	double e0     = 0.0611 * hum * pow(10, atkel);
	double tksea  = tkel - tlapse * htkel;
	double em     = -978.77 / (2.8704e6 * tlapse * 1.0e-5);
	double tkelh  = tksea + tlapse * hhum;
	double e0sea  = e0 * pow((tksea/tkelh), (4*em));
	double tkelp  = tksea + tlapse * hp;
	double psea   = p * pow((tksea/tkelp), em);
	if(sinel < 0)
	{
	    sinel = 0;
	}
	double tropo = 0;
	int done = FALSE_FLAG;
	double refsea  = 77.624e-6 / tksea;
	double htop    = 1.1385e-5 / refsea;
	refsea  = refsea * psea;
	double ref = refsea * pow(((htop - hsta) / htop), 4);
	int i;
	while(1)
	{
	    double rtop = pow((a_e + htop), 2) - pow((a_e + hsta), 2) * (1 - pow(sinel, 2));
	    // check to see if geometry is crazy
	    if(rtop < 0)
	    {
	        rtop = 0;
	    }
	    rtop = sqrt(rtop) - (a_e + hsta) * sinel;
	    double a    = -sinel / (htop - hsta);
	    double b    = -b0 * (1-pow(sinel, 2)) / (htop - hsta);
	    double rn[8*1];
	    for(i=0;i<8;i++)
	    {
	    	rn[i] = pow(rtop, (i+2));
	    }
	    double alpha[1*8];
	    alpha[0] = 2 * a;
	    alpha[1] = 2 * pow(a, 2) + 4 * b / 3;
	    alpha[2] = a * (pow(a, 2) + 3 * b);
	    alpha[3] = pow(a, 4) / 5 + 2.4 * pow(a, 2) * b + 1.2 * pow(b, 2);
	    alpha[4] = 2 * a * b * (pow(a, 2) + 3 * b) / 3;
	    alpha[5] = pow(b, 2) * ( 6 * pow(a, 2) + 4 * b) * 1.428571e-1;
	    alpha[6] = 0;
	    alpha[7] = 0;
	    if(pow(b, 2) > 1.0e-35)
	    {
	        alpha[6] = a * pow(b, 3) / 2;
	        alpha[7] = pow(b, 4) / 9;
	    }
	    double alphaByrn[1];
	    MatrixProduct(alpha, 1, 8, rn, 8, 1, alphaByrn);
	    double dr = rtop;
	    dr = dr + alphaByrn[0];
	    tropo = tropo + dr * ref * 1000;
	    if(done == TRUE_FLAG)
	    {
	        return tropo;
	    }
	    done    = TRUE_FLAG;
	    refsea  = (371900.0e-6 / tksea - 12.92e-6) / tksea;
	    htop    = 1.1385e-5 * (1255 / tksea + 0.05) / refsea;
	    ref     = refsea * e0sea * pow(((htop-hsta) / htop), 4);
	}
	return tropo;
}

void EarthCorrection(double travelTime, double *x, double *rotX, int numOfSat, int channel)
{
	double omegatau;
	double r3[3*3];
	double xCorr[3];
	double xSatRot[3];

	omegatau = OMEGAE_DOT * travelTime;
	r3[0] = cos(omegatau);
	r3[1] = sin(omegatau);
	r3[2] = 0;
	r3[3] = -sin(omegatau);
	r3[4] = cos(omegatau);
	r3[5] = 0;
	r3[6] = 0;
	r3[7] = 0;
	r3[8] = 1;
	xCorr[0] = x[channel+(numOfSat*0)];
	xCorr[1] = x[channel+(numOfSat*1)];
	xCorr[2] = x[channel+(numOfSat*2)];
	MatrixProduct(r3, 3, 3, xCorr, 3, 1, xSatRot);
	rotX[channel+(numOfSat*0)] = xSatRot[0];
	rotX[channel+(numOfSat*1)] = xSatRot[1];
	rotX[channel+(numOfSat*2)] = xSatRot[2];
}

void Topocent(double *ecefCoord, double *satEcef, double *satAngle)
{
	double dtr;
	double cl, sl, cb, sb, enuE, enuN, enuU, az, el, d;
	double horDis;
	double ff[3][3];
	double xGeod[3];
	double dxEcefMat[3][1];
	double localVectorMat[3][1];
	int j;
	double dxEcef[3];

	dxEcef[0] = satEcef[0] - ecefCoord[0];
	dxEcef[1] = satEcef[1] - ecefCoord[1];
	dxEcef[2] = satEcef[2] - ecefCoord[2];

	dtr = DEGREE_TO_RADIAN; /* PI/180; */

	// Trasform from ECEF into geodedic
	Ecef2Geod(ecefCoord, xGeod);


	// X_geod[0] is the lat (phi)
	// X_geod[1] is the long (lambda)
	cl = cos(xGeod[0] * dtr);
	sl = sin(xGeod[0] * dtr);
	cb = cos(xGeod[1] * dtr);
	sb = sin(xGeod[1] * dtr);

	ff[0][0] = -sl ;
	ff[1][0] = -sb*cl;
	ff[2][0] = cb*cl;
	ff[0][1] = cl;
	ff[1][1] = -sb*sl;
	ff[2][1] = cb*sl;
	ff[0][2] = 0;
	ff[1][2] = cb;
	ff[2][2] = sb;

	for(j=0; j<3; j++)
	{
		dxEcefMat[j][0] = dxEcef[j];
	}

	MatrixProduct(ff[0], 3, 3, dxEcefMat[0], 3, 1, localVectorMat[0]);

	enuE = localVectorMat[0][0];
	enuN = localVectorMat[1][0];
	enuU = localVectorMat[2][0];

	horDis = sqrt(enuE * enuE + enuN * enuN);

	if(horDis < 1E-20)
	{
		az = 0;
		el = 90;
    }
	else
	{
		az = atan2(enuE, enuN)/dtr;
		el = atan2(enuU, horDis)/dtr;
    }

	if(az < 0)
	{
		az = az + 360;
    }

	d = sqrt(dxEcef[0]*dxEcef[0] + dxEcef[1]*dxEcef[1] + dxEcef[2]*dxEcef[2]);

	satAngle[0] = el;
	satAngle[1] = az;
	satAngle[2] = d;
}


void GeodToEcef(double *geodCoord,double *ecefCoord)
{
	double sLat, cLat, sLon, cLon, h;
	double re;

	sLat=sin(geodCoord[1]*DEGREE_TO_RADIAN);
	cLat=cos(geodCoord[1]*DEGREE_TO_RADIAN);
	sLon=sin(geodCoord[0]*DEGREE_TO_RADIAN);
	cLon=cos(geodCoord[0]*DEGREE_TO_RADIAN);
	h=geodCoord[2];

	re=A_CONST/(sqrt(1.0 - (E_SQUARE_CONST*sLat*sLat)));
	ecefCoord[0] = (re+h)*cLat*cLon;
	ecefCoord[1] = (re+h)*cLat*sLon;
	ecefCoord[2] = (re*(1-E_SQUARE_CONST)+h)*sLat;
}


void Ecef2Geod(double *ecefCoord, double *geodCoord)
{
	double b, ep, p, th, lon, lat, n, alt;
	double x, y, z;

	x = ecefCoord[0];
	y = ecefCoord[1];
	z = ecefCoord[2];

	// calculations:
	b   = sqrt(A_CONST*A_CONST*(1-E_CONST*E_CONST));
	ep  = sqrt((A_CONST*A_CONST-b*b)/(b*b));

	p   = sqrt(x*x + y*y);
	th  = atan2(A_CONST*z,b*p);
	lon = atan2(y,x);
	lat = atan2((z + ep*ep*b*(sin(th)*sin(th)*sin(th))),(p-E_CONST*E_CONST*A_CONST*(cos(th)*cos(th)*cos(th))));
	n   = A_CONST/sqrt(1-E_CONST*E_CONST*(sin(lat)*sin(lat)));
	alt = (p/cos(lat))-n;

	lon = fmod(lon,TWICE_M_PI);

	if((fabs(x)<1) && (fabs(y)<1))
	{
		alt = fabs(z)-b;
	}

	lat = lat*RADIAN_TO_DEGREE; /* lat*180/PI; */
	lon = lon*RADIAN_TO_DEGREE; /* lon*180/PI; */

	if(lon>180)
	{
	    lon = lon - 360;
	}

	if(lon<-180)
	{
	    lon = lon + 360;
	}

	geodCoord[0] = lon;
	geodCoord[1] = lat;
	geodCoord[2] = alt;
}


void LlaToUtm(double lat, double lon, double alt, double *enu, double *utmZone)
{
	double f;
	double e2;
	double a;
	double ko, No, Eo, Zone, lcm, lam;
	int min0,mag60;
	double RN, t, h, a0, a2, a4, a6, a8, s, n;
	double E1, E2, E3, E4, E;
	double N1, N2, N3, N4, N;

	e2   = 0.00669437999014;
	a = 6378137;	// semi-major axis
	ko=0.9996;      // Scale factor
	if(lat>=0)
	{
		No=0;
	}		        // False northing (north)
	else
	{
		No=10000000;  // False northing (south)
	}
	Eo=500000;      // False easting
	Zone = floor(((180/M_PI)*(lon)-180)/6)+1;

	min0 = 0;
	mag60 = 0;
	if(Zone < 0)
	{
		min0 = 1;
	}
	if(Zone > 60)
	{
		mag60 = 1;
	}
	Zone = Zone + min0 * 60-mag60 * 60;
	lcm = DEGREE_TO_RADIAN*(Zone*6-183);
	lam = lon-lcm;
	if(lam >= M_PI)
	{
		lam = lam - TWICE_M_PI;
	}
	f = 1-sqrt(1-e2);
	RN = a /pow(1-e2*sin(lat)*sin(lat), 0.5);
	t = tan(lat);
	h = sqrt(e2*cos(lat)*cos(lat)/(1-e2));
	n = f/(2-f);

	a0 = 1 + (n*n)/4 + (n*n*n*n)/64;
	a2 = 1.5*(n-(n*n*n)/8);
	a4 = (15.0/16.0)*(n*n-(n*n*n*n)/4);
	a6 = (35.0/48.0)*n*n*n;
	a8 = (315.0/512.0)*n*n*n*n;

	s = (a /(1+n))*(a0*lat-a2*sin(2*lat)+a4*sin(4*lat)- a6*sin(6*lat)+a8*sin(8*lat));

	E1 = lam * cos(lat);
	E2 = ((lam * lam * lam)* (cos(lat) * cos(lat) * cos(lat)) * (1-t*t + h*h))/6;
	E3=(lam*lam*lam*lam*lam) * (cos(lat)* cos(lat)* cos(lat)* cos(lat)* cos(lat))* (5.0-18.0*t*t + t*t*t*t + 14.0*h*h - 58.0*t*t*h*h + 13*h*h*h*h + 4*h*h*h*h*h*h - 64.0*t*t*h*h*h*h - 24.0*t*t*h*h*h*h*h*h) /120.0;
	E4 =(lam*lam*lam*lam*lam*lam*lam) *(cos(lat)* cos(lat)* cos(lat)* cos(lat)* cos(lat) * cos(lat) * cos(lat)) * (61.0-479.0*t*t + 179.0*t*t*t*t - t*t*t*t*t*t) /5040.0;
	E = Eo + ko*RN*(E1 + E2 + E3 + E4);

	N1 = 0.5 * lam*lam * sin(lat) * cos(lat);
	N2 = (lam*lam*lam*lam/24.0) * sin(lat) * cos(lat) * cos(lat) * cos(lat) * (5.0-t*t + 9.0*h*h + 4.0*h*h*h*h);
	N3=(lam*lam*lam*lam*lam*lam/720) * sin(lat) * cos(lat) * cos(lat) * cos(lat) * cos(lat) *cos(lat)* (61.0-58.0*t*t + t*t*t*t + 270.0*h*h - 330.0*t*t*h*h + 445.0*h*h*h*h + 324.0*h*h*h*h*h*h - 680.0*t*t*h*h*h*h + 88.0*h*h*h*h*h*h*h*h-600.0*t*t*h*h*h*h*h*h - 192.0*t*t*h*h*h*h*h*h*h*h);
	N4=(lam*lam*lam*lam*lam*lam*lam*lam/40320.0) * sin(lat) * cos(lat)* cos(lat) * cos(lat) * cos(lat) * cos(lat) *cos(lat)* cos(lat) * (1385.0-311.0*t*t + 543.0*t*t*t*t - t*t*t*t*t*t);
	N = No + ko*RN*((s/RN) + N1 + N2 + N3 + N4);

	enu[0] = E;
	enu[1] = N;
	enu[2] = alt;
	*utmZone = Zone;
}


void EcefVelToEnuVel(double *ecefPos,double *ecefVel,double *enuPos,double *enuVel)
{
	double deltaT = 0.01;
	/* next position computed using the velocity out from the PVT */
	double ecefU2[3];
	double llaU2[3];
	double enuU2[3];
	double utmZone2;

	// Project ahead the user position in ECEF
	ecefU2[0] = ecefPos[0] + (deltaT * ecefVel[0]);
	ecefU2[1] = ecefPos[1] + (deltaT * ecefVel[1]);
	ecefU2[2] = ecefPos[2] + (deltaT * ecefVel[2]);
	/* Transform from ECEF to ENUenu */
	Ecef2Geod(ecefU2,llaU2);
	LlaToUtm(DEGREE_TO_RADIAN*llaU2[1],DEGREE_TO_RADIAN*llaU2[0],llaU2[2],enuU2,&utmZone2);
	/* compute velocity */
	enuVel[0] = (enuU2[0] - enuPos[0]) / deltaT;
	enuVel[1] = (enuU2[1] - enuPos[1]) / deltaT;
	enuVel[2] = (enuU2[2] - enuPos[2]) / deltaT;
}


/* constants and function used to find the geoid separation */

#define GEOID_ROW	19
#define GEOID_COL	37

static int GEOID_DELTA[] = {-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30, -30,-30,-30,-30,-30,-30,-30,-30,-30,-30,-30,
				-53,-54,-55,-52,-48,-42,-38,-38,-29,-26,-26,-24,-23,-21,-19,-16,-12, -8, -4, -1,  1,  4,  4,  6,  5,  4,   2, -6,-15,-24,-33,-40,-48,-50,-53,-52,-53,
				-61,-60,-61,-55,-49,-44,-38,-31,-25,-16, -6,  1,  4,  5,  4,  2,  6, 12, 16, 16, 17, 21, 20, 26, 26, 22,  16, 10, -1,-16,-29,-36,-46,-55,-54,-59,-61,
				-45,-43,-37,-32,-30,-26,-23,-22,-16,-10, -2, 10, 20, 20, 21, 24, 22, 17, 16, 19, 25, 30, 35, 35, 33, 30,  27, 10, -2,-14,-23,-30,-33,-29,-35,-43,-45,
				-15,-18,-18,-16,-17,-15,-10,-10, -8, -2,  6, 14, 13,  3,  3, 10, 20, 27, 25, 26, 34, 39, 45, 45, 38, 39,  28, 13, -1,-15,-22,-22,-18,-15,-14,-10,-15,
				21,  6,  1, -7,-12,-12,-12,-10, -7, -1,  8, 23, 15, -2, -6,  6, 21, 24, 18, 26, 31, 33, 39, 41, 30, 24,  13, -2,-20,-32,-33,-27,-14, -2,  5, 20, 21,
				46, 22,  5, -2, -8,-13,-10, -7, -4,  1,  9, 32, 16,  4, -8,  4, 12, 15, 22, 27, 34, 29, 14, 15, 15,  7,  -9,-25,-37,-39,-23,-14, 15, 33, 34, 45, 46,
				51, 27, 10,  0, -9,-11, -5, -2, -3, -1,  9, 35, 20, -5, -6, -5,  0, 13, 17, 23, 21,  8, -9,-10,-11,-20, -40,-47,-45,-25,  5, 23, 45, 58, 57, 63, 51,
				36, 22, 11,  6, -1, -8,-10, -8,-11, -9,  1, 32,  4,-18,-13, -9,  4, 14, 12, 13, -2,-14,-25,-32,-38,-60, -75,-63,-26,  0, 35, 52, 68, 76, 64, 52, 36,
				22, 16, 17, 13,  1,-12,-23,-20,-14, -3, 14, 10,-15,-27,-18,  3, 12, 20, 18, 12,-13, -9,-28,-49,-62,-89,-102,-63, -9, 33, 58, 73, 74, 63, 50, 32, 22,
				13, 12, 11,  2,-11,-28,-38,-29,-10,  3,  1,-11,-41,-42,-16,  3, 17, 33, 22, 23,  2, -3, -7,-36,-59,-90, -95,-63,-24, 12, 53, 60, 58, 46, 36, 26, 13,
				5, 10,  7, -7,-23,-39,-47,-34, -9,-10,-20,-45,-48,-32, -9, 17, 25, 31, 31, 26, 15,  6,  1,-29,-44,-61, -67,-59,-36,-11, 21, 39, 49, 39, 22, 10,  5,
				-7, -5, -8,-15,-28,-40,-42,-29,-22,-26,-32,-51,-40,-17, 17, 31, 34, 44, 36, 28, 29, 17, 12,-20,-15,-40, -33,-34,-34,-28,  7, 29, 43, 20,  4, -6, -7,
				-12,-10,-13,-20,-31,-34,-21,-16,-26,-34,-33,-35,-26,  2, 33, 59, 52, 51, 52, 48, 35, 40, 33, -9,-28,-39, -48,-59,-50,-28,  3, 23, 37, 18, -1,-11,-12,
				-8,  8,  8,  1,-11,-19,-16,-18,-22,-35,-40,-26,-12, 24, 45, 63, 62, 59, 47, 48, 42, 28, 12,-10,-19,-33, -43,-42,-43,-29, -2, 17, 23, 22,  6,  2, -8,
				2,  9, 17, 10, 13,  1,-14,-30,-39,-46,-42,-21,  6, 29, 49, 65, 60, 57, 47, 41, 21, 18, 14,  7, -3,-22, -29,-32,-32,-26,-15, -2, 13, 17, 19,  6,  2,
				2,  2,  1, -1, -3, -7,-14,-24,-27,-25,-19,  3, 24, 37, 47, 60, 61, 58, 51, 43, 29, 20, 12,  5, -2,-10, -14,-12,-10,-14,-12, -6, -2,  3,  6,  4,  2,
				3,  1, -2, -3, -3, -3, -1,  3,  1,  5,  9, 11, 19, 27, 31, 34, 33, 34, 33, 34, 28, 23, 17, 13,  9,  4,   4,  1, -2, -2,  0,  2,  3,  2,  1,  1,  3,
				13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,  13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13};



double GetWgs84Separation(double lat,double lon)
{
	int ilatitude = (int)floor((90 +  lat)/10);
	int ilongitude = (int)floor((180 + lon)/10);
	int ilatitude2 = 0, ilongitude2 = 0;
	double altitude = 0, delta = 0;

	if(ilatitude < (GEOID_ROW-1))
	{
		ilatitude2 = ilatitude+1;
	}
	else
	{
		ilatitude2 = ilatitude;
	}

	if(ilongitude < (GEOID_COL-1))
	{
		ilongitude2 = ilongitude+1;
	}
	else
	{
		ilongitude2 = ilongitude;
	}

	double x1  =   ilongitude*10-180;
	double y1  =   ilatitude*10-90;
	double x2  =   ilongitude2*10-180;
	double y2  =   ilatitude2*10-90;
	double x   =   lon;
	double y   =   lat;
	double z11 =   GEOID_DELTA[ilongitude + ilatitude*GEOID_COL ];
	double z12 =   GEOID_DELTA[ilongitude2 + ilatitude*GEOID_COL ];
	double z21 =   GEOID_DELTA[ilongitude + ilatitude2*GEOID_COL ];
	double z22 =   GEOID_DELTA[ilongitude2 + ilatitude2*GEOID_COL ];


	delta = (y2-y1)*(x2-x1);

	altitude = ( z22*(y-y1)*(x-x1) + z12*(y2-y)*(x-x1) + z21*(y-y1)*(x2-x) + z11*(y2-y)*(x2-x) )/delta;

	return altitude;
}


