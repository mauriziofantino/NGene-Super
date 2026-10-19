/**
 * @file egnosIonoCorrections.c
 *
 * @author Mario Nicola
 *
 * @date 21/08/2015
 *
 * @version 1.0.0
 *
 * @brief Functions used to manage the EGNOS ionospheric corrections.
 */

#include <math.h>
#include <stdlib.h>
#include "ngeneConstants.h"
#include "egnosCorrectionsDataStructure.h"
#include "egnosIonoMaskBandTable.h"
#include "ngeneConfiguration.h"
#include "arrays.h"
#include "egnosIonoCorrections.h"


static const int egnosIonoMaskBandsTable[2192][4] = EGNOS_IONO_MASK_BAND_VALUES;

void GetIgpLatLon(int band, int idx, int *lat, int *lon)
{
    int tabIndex;

    if(band < 9)
    {
		tabIndex = idx + (band * 201) + (0 * 200) + (0 * 192);
		if((egnosIonoMaskBandsTable[tabIndex][0] == band) && (idx == (egnosIonoMaskBandsTable[tabIndex][1] - 1)))
		{
			*lat = egnosIonoMaskBandsTable[tabIndex][2];
			*lon = egnosIonoMaskBandsTable[tabIndex][3];
		}
    }
    else if(band == 9)
	{
		tabIndex = idx + (8 * 201) + (1 * 200) + (0 * 192);
		if ((egnosIonoMaskBandsTable[tabIndex][0] == band) && (idx == (egnosIonoMaskBandsTable[tabIndex][1] - 1)))
		{
			*lat = egnosIonoMaskBandsTable[tabIndex][2];
			*lon = egnosIonoMaskBandsTable[tabIndex][3];
		}
	}
    else if(band == 10)
	{
		tabIndex = idx + (8 * 201) + (1 * 200) + (1 * 192);
		if((egnosIonoMaskBandsTable[tabIndex][0] == band) && (idx == (egnosIonoMaskBandsTable[tabIndex][1] - 1)))
		{
			*lat = egnosIonoMaskBandsTable[tabIndex][2];
			*lon = egnosIonoMaskBandsTable[tabIndex][3];
		}
	}
}


int CorrectLon(int lon)
{
	if(lon<-180)
	{
		lon += 360;
	}
	else if(lon>=180)
	{
		lon -= 360;
	}
	return lon;
}


#define LON_INDEX	0
#define LAT_INDEX	1

void GetIonosphericPiercePoint(double *userPos,double az,double el,double *ippLat,double *ippLon,double *fpp)
{
	double userPosRad[2];
	const double re = 6378136.3; /* approximate radius of the earth’s ellipsoid */
	const double hi =  350000.0; /* height of the maximum electron density */
	double psiPp; /* earth's central angle between the user position and the earth projection of the pierce point */
	double phiPp; /* IPP latitude [rad] */
	double lambdaPp; /* IPP longitude [rad] */

    /* Trasform from degree to radian */
    userPosRad[LON_INDEX] = userPos[LON_INDEX] * DEGREE_TO_RADIAN;
    userPosRad[LAT_INDEX] = userPos[LAT_INDEX] * DEGREE_TO_RADIAN;

    /* Calculate earth's central angle between the user position and the earth projection of the Pierce Point */
    psiPp = M_PI_2 - el - asin((re/(re+hi)) * cos(el));

    /* Calculate Pierce Point Latitude */
    phiPp = asin((sin(userPosRad[LAT_INDEX])*cos(psiPp)) + (cos(userPosRad[LAT_INDEX])*sin(psiPp)*cos(az)));

    /* Calculate Pierce Point Longitude */
    if (((userPos[LAT_INDEX]> 70.0) && ((tan(psiPp) * cos(az)) > tan(M_PI_2 - userPosRad[LAT_INDEX])))  ||
        ((userPos[LAT_INDEX]<-70.0) && ((tan(psiPp) * cos(az+M_PI)) > tan(M_PI_2 + userPosRad[LAT_INDEX]))))
    {
    	/* TODO in PROSA la seconda condizione è scritta:
    	 * ((userPos[LAT_INDEX]<-70.0) && ((-tan(psiPp) * cos(az+M_PI)) > tan(-M_PI_2 - userPosRad[LAT_INDEX])))
    	 * nella documentazione come implementata qui sopra: perchè?
    	 */
    	lambdaPp = userPosRad[LON_INDEX] + M_PI - asin((sin(psiPp)*sin(az))/cos(phiPp));
    }
    else
    {
    	lambdaPp = userPosRad[LON_INDEX] + asin((sin(psiPp)*sin(az))/cos(phiPp));
    }
    *ippLat = phiPp * RADIAN_TO_DEGREE;
    *ippLon = lambdaPp * RADIAN_TO_DEGREE;
    *fpp = 1.0 / sqrt(1.0 - pow((re/(re+hi)) * cos(el),2.0));
}

/* Functions used to get ionospheric corrections in function of the pierce point */

int GetMaskIndex(TEgnosCorrections *egnosCorr,int lat,int lon,int bandNumber)
{
	TEgnosIonoMask *ionoMask;
	int maskIndex;

	ionoMask = &(egnosCorr->ionoMask[bandNumber]);
	maskIndex = 0;
	while((maskIndex<(int)ionoMask->numOfPointInTheMask) &&	((ionoMask->pointLatitudes[maskIndex]!=lat) || (ionoMask->pointLongitudes[maskIndex]!=lon)))
	{
		maskIndex++;
	}
	if(maskIndex<(int)ionoMask->numOfPointInTheMask)
	{
		return maskIndex;
	}
	return INVALID_VALUE;
}

void GetIgpInformation(TEgnosCorrections *egnosCorr,int lat,int lon,int *bandNumberPtr,int *maskIndexPtr,double *vertDelay)
{
	int bandNumber;
	TEgnosIonoMask *ionoMask;
	int maskIndex;
	TEgnosIonoIgpDelayCorrections *igpIonoCorr;

	*bandNumberPtr = INVALID_VALUE;
	*maskIndexPtr = INVALID_VALUE;
	*vertDelay = 0.0;

	/* try bands from 0 to 8 */
	bandNumber = ((int)floor(lon)+180)/40;
	ionoMask = &(egnosCorr->ionoMask[bandNumber]);
	maskIndex = GetMaskIndex(egnosCorr,lat,lon,bandNumber);
	if(maskIndex!=INVALID_VALUE)
	{
		/* found in the selected mask */
		igpIonoCorr = &(egnosCorr->bandIonoCorrections[bandNumber].igpIonoCorrections[maskIndex]);
		if((igpIonoCorr->valid==TRUE_FLAG) && (igpIonoCorr->iodi==ionoMask->maskIodi) && (igpIonoCorr->usable==TRUE_FLAG) && (igpIonoCorr->monitored==TRUE_FLAG))
		{
			*bandNumberPtr = bandNumber;
			*maskIndexPtr = maskIndex;
			*vertDelay = igpIonoCorr->igpVerticalDelay;
		}
		return;
	}
	if((lat>-60) && (lat<60))
	{
		/* cannot be found */
		return;
	}

	/* try bands 9 and 10 */
	if(lat>0)
	{
		bandNumber = 9;
	}
	else
	{
		bandNumber = 10;
	}
	ionoMask = &(egnosCorr->ionoMask[bandNumber]);
	maskIndex = GetMaskIndex(egnosCorr,lat,lon,bandNumber);
	if(maskIndex!=INVALID_VALUE)
	{
		/* found in the selected mask */
		igpIonoCorr = &(egnosCorr->bandIonoCorrections[bandNumber].igpIonoCorrections[maskIndex]);
		if((igpIonoCorr->valid==TRUE_FLAG) && (igpIonoCorr->iodi==ionoMask->maskIodi) && (igpIonoCorr->usable==TRUE_FLAG) && (igpIonoCorr->monitored==TRUE_FLAG))
		{
			*bandNumberPtr = bandNumber;
			*maskIndexPtr = maskIndex;
			*vertDelay = igpIonoCorr->igpVerticalDelay;
		}
		return;
	}
	/* not found in the mask! */
}


void ResetIgpInfo(TIgpInfo *ptr)
{
	ArrayClean((char *)ptr->lat,4*sizeof(int));
	ArrayClean((char *)ptr->lon,4*sizeof(int));
	ptr->lat1 = 0;
	ptr->lat2 = 0;
	ptr->lon1 = 0;
	ptr->lon2 = 0;
	ArrayClean((char *)ptr->bandNumber,4*sizeof(int));
	ArrayClean((char *)ptr->maskIndex,4*sizeof(int));
	ArrayClean((char *)ptr->verticalDelay,4*sizeof(double));
	ptr->validElements = 0;
}

void CopyIgpInfo(TIgpInfo *src,TIgpInfo *dest)
{
	ArrayCopy((char *)src->lat,(char *)dest->lat,4*sizeof(int));
	ArrayCopy((char *)src->lon,(char *)dest->lon,4*sizeof(int));
	dest->lat1 = src->lat1;
	dest->lat2 = src->lat2;
	dest->lon1 = src->lon1;
	dest->lon2 = src->lon2;
	ArrayCopy((char *)src->bandNumber,(char *)dest->bandNumber,4*sizeof(int));
	ArrayCopy((char *)src->maskIndex,(char *)dest->maskIndex,4*sizeof(int));
	ArrayCopy((char *)src->verticalDelay,(char *)dest->verticalDelay,4*sizeof(double));
	dest->validElements = src->validElements;
}

/*
 * Ordina i vertici di un triangolo descritto da una struttura TIgpInfo:
 * - il vertice dell'angolo retto viene messo all'indice 1
 *   (standard: "the grid point 2 [nel documento l'indice parte da 1 NdMN] is always the vertex opposite to the hypotenuse")
 * - il vertice alla stessa longitudine rispetto al precedente (indice 1) viene messo all'indice 0
 * - il vertice alla stessa latitudine rispetto al precedente (indice 1) viene messo all'indice 2
 *
 * Il vertice dell'angolo retto viene individuato cercando quale spigolo manca per formare il quadrato/rettangolo.
 *
 * Nella struttura TIgpInfo in ingresso i vertici sono ordinati come da standard (page A-38)
 **/
void SortIgpsTriangle(TIgpInfo *igps,TIgpInfo *sortedIgps)
{
	int igpIdx;
	int invalidIndex = INVALID_VALUE;
	int sortedIndexes[3];
	int srcIdx;

	ResetIgpInfo(sortedIgps);
	if(igps->validElements!=3)
	{
		return;
	}

	/* Find the invalid IGP */
	for(igpIdx=0;igpIdx<4;igpIdx++)
	{
		if((igps->bandNumber[igpIdx]==INVALID_VALUE) || (igps->maskIndex[igpIdx]==INVALID_VALUE))
		{
			invalidIndex = igpIdx;
		}
	}
	/* Sort */
	switch(invalidIndex)
	{
		case 0:
			sortedIndexes[0] = 1;
			sortedIndexes[1] = 2;
			sortedIndexes[2] = 3;
			break;
		case 1:
			sortedIndexes[0] = 0;
			sortedIndexes[1] = 3;
			sortedIndexes[2] = 2;
			break;
		case 2:
			sortedIndexes[0] = 3;
			sortedIndexes[1] = 0;
			sortedIndexes[2] = 1;
			break;
		case 3:
		default:
			sortedIndexes[0] = 2;
			sortedIndexes[1] = 1;
			sortedIndexes[2] = 0;
			break;
	}
	/* Save */
	sortedIgps->validElements = igps->validElements;
	sortedIgps->lat1 = igps->lat1;
	sortedIgps->lat2 = igps->lat2;
	sortedIgps->lon1 = igps->lon1;
	sortedIgps->lon2 = igps->lon2;
	for(igpIdx=0;igpIdx<3;igpIdx++)
	{
		srcIdx = sortedIndexes[igpIdx];
		sortedIgps->lat[igpIdx] = igps->lat[srcIdx];
		sortedIgps->lon[igpIdx] = igps->lon[srcIdx];
		sortedIgps->bandNumber[igpIdx] = igps->bandNumber[srcIdx];
		sortedIgps->maskIndex[igpIdx] = igps->maskIndex[srcIdx];
		sortedIgps->verticalDelay[igpIdx] = igps->verticalDelay[srcIdx];
	}
}

/*
 * Verifica se il pierce point è dentro il triangolo descritto dalla struttura TIgpInfo.
 * I vertici del triangolo nella struttura TIgpInfo vanno prima ordinati con SortIgpsTriangle.
 **/
int CheckTriangle(TIgpInfo *igps,double ippLat,double ippLon)
{
	double m = (double)(igps->lat[2]-igps->lat[0])/(double)(igps->lon[2]-igps->lon[0]);
	double lat = (double)igps->lat[0] + (m*(double)(ippLon-igps->lon[0]));

	if((igps->lat[0]>igps->lat[1]) && (ippLat<=lat))
		return TRUE_FLAG;

	if((igps->lat[0]<igps->lat[1]) && (ippLat>=lat))
		return TRUE_FLAG;

	return FALSE_FLAG;
}


void SelectIgps(TEgnosCorrections *egnosCorr,TIgpInfo *refIgps,double ippLat,double ippLon,int deltaLat,int deltaLon)
{
	int gridStep = 5;					/* 5 is the basic step in the iono grid */
	int numOfLons = deltaLon/gridStep;	/* this is the number of possible longitudes to be considered */
	int numOfLats = deltaLat/gridStep;	/* this is the number of possible latitudes to be considered */
	int numOfSets = numOfLons*numOfLats;
	TIgpInfo *allIgps;
	int idxLon;
	int idxLat;
	TIgpInfo *newIgps;
	int igpIdx;
	int lon;
	int lat;
	unsigned char doNotUse;
	int setIdx;
	TIgpInfo sortedIgps;

	ResetIgpInfo(refIgps);
	allIgps = (TIgpInfo *)malloc(numOfSets*sizeof(TIgpInfo));

	/* list and test all possible squares with size (deltaLon X deltaLat) including the IPP */
	for(idxLat=0;idxLat<numOfLats;idxLat++)
	{
		for(idxLon=0;idxLon<numOfLons;idxLon++)
		{
			doNotUse = FALSE_FLAG;
			newIgps = &(allIgps[(idxLat*numOfLons)+idxLon]);
			newIgps->lat1 = (gridStep * (int)floor(ippLat/(double)gridStep)) - (idxLat*gridStep);
			newIgps->lat2 = newIgps->lat1 + deltaLat;
			newIgps->lat[0] = newIgps->lat[1] = newIgps->lat2;
			newIgps->lat[2] = newIgps->lat[3] = newIgps->lat1;
			newIgps->lon1 = (gridStep * (int)floor(ippLon/(double)gridStep)) - (idxLon*gridStep);
			newIgps->lon2 = newIgps->lon1 + deltaLon;
			newIgps->lon[0] = newIgps->lon[3] = newIgps->lon2;
			newIgps->lon[1] = newIgps->lon[2] = newIgps->lon1;
			newIgps->validElements = 0;
			for(igpIdx=0;igpIdx<4;igpIdx++)
			{
				lon = CorrectLon(newIgps->lon[igpIdx]);
				lat = newIgps->lat[igpIdx];
				GetIgpInformation(egnosCorr,lat,lon,&(newIgps->bandNumber[igpIdx]),&(newIgps->maskIndex[igpIdx]),&(newIgps->verticalDelay[igpIdx]));
				if((newIgps->bandNumber[igpIdx]!=INVALID_VALUE) && (newIgps->maskIndex[igpIdx]!=INVALID_VALUE))
				{
					newIgps->validElements++;
				}
				if(newIgps->verticalDelay[igpIdx]>VERTICAL_DELAY_LIMIT)
				{
					doNotUse = TRUE_FLAG;
				}
			}
			if(doNotUse==TRUE_FLAG)
			{
				newIgps->validElements = 0;
			}
			/* If 4 IGPs are available, then the points are ready for use */
			if(newIgps->validElements==4)
			{
				CopyIgpInfo(newIgps,refIgps);
				free(allIgps);
				return;
			}
		}
	}

	/* If a square is not available, look for triangles (3 IGPs are available) */
	setIdx=0;
	while(setIdx<numOfSets)
	{
		newIgps = &(allIgps[setIdx]);
		if(newIgps->validElements==3)
		{
			/* If less than 3 IGPs are available, then a valid reference set is not available */
			/* If 3 IGPs are available, then the points must be sorted before use! */
			SortIgpsTriangle(newIgps,&sortedIgps);
		}
		/* Check both the number of points and the presence of IPP inside the triangle */
		if((newIgps->validElements==3) && (CheckTriangle(&sortedIgps,ippLat,ippLon)==TRUE_FLAG))
		{
			CopyIgpInfo(&sortedIgps,refIgps);
			setIdx = numOfSets;
		}
		else
		{
			setIdx++;
		}
	}
	free(allIgps);
}


int VerticalDelayInterpolation(double ippLat,double ippLon,double fpp,TIgpInfo *refIgps,double *verticalDelayPtr)
{
	double xpp;
	double ypp;
	double w[4];
	double verticalDelay = 0.0;
	int idx;

	/* four-point interpolation */
	if(refIgps->validElements==4)
	{
		xpp = (ippLon-(double)refIgps->lon1)/((double)refIgps->lon2-(double)refIgps->lon1);
		ypp = (ippLat-(double)refIgps->lat1)/((double)refIgps->lat2-(double)refIgps->lat1);
		w[0] = xpp*ypp;
		w[1] = (1.0-xpp)*ypp;
		w[2] = (1.0-xpp)*(1.0-ypp);
		w[3] = xpp*(1.0-ypp);
		for(idx=0;idx<4;idx++)
		{
			verticalDelay += (w[idx]*refIgps->verticalDelay[idx]);
		}
		verticalDelay *= fpp; /* Sign changes w.r.t. the standard, because this correction is subtracted */
		*verticalDelayPtr = verticalDelay;
		return TRUE_FLAG;
	}
	/* three-point interpolation */
	if(refIgps->validElements==3)
	{
		xpp = fabs(ippLon-(double)refIgps->lon[1])/((double)refIgps->lon2-(double)refIgps->lon1);
		ypp = fabs(ippLat-(double)refIgps->lat[1])/((double)refIgps->lat2-(double)refIgps->lat1);
		w[0] = ypp;
		w[1] = 1.0 - xpp - ypp;
		w[2] = xpp;
		for(idx=0;idx<3;idx++)
		{
			verticalDelay += (w[idx]*refIgps->verticalDelay[idx]);
		}
		verticalDelay *= fpp; /* Sign changes w.r.t. the standard, because this correction is subtracted */
		*verticalDelayPtr = verticalDelay;
		return TRUE_FLAG;
	}
	return FALSE_FLAG;
}


void SelectIgpsCaseCNorth(TEgnosCorrections *egnosCorr,TIgpInfo *refIgps,double ippLon)
{
	int idx;
	int lat;
	int lon;
	const int delta = 10;
	const int altBandNumber = 9;
	int altDelta;
	int lon85[2];
	double delay85[2];
	int maskIndex;
	TEgnosIonoMask *ionoMask;
	TEgnosIonoIgpDelayCorrections *igpIonoCorr;
	double coeff;

	refIgps->lat1 = 75;
	refIgps->lat2 = 85;
	refIgps->lon1 = delta * (int)floor(ippLon/(double)delta);
	refIgps->lon2 = refIgps->lon1 + delta;
	refIgps->validElements = 0;
	refIgps->lat[0] = refIgps->lat[1] = refIgps->lat2;
	refIgps->lat[2] = refIgps->lat[3] = refIgps->lat1;
	refIgps->lon[1] = refIgps->lon[2] = refIgps->lon1;
	refIgps->lon[0] = refIgps->lon[3] = refIgps->lon2;
	/* find the first two points (75°) */
	for(idx=2;idx<4;idx++)
	{
		lon = CorrectLon(refIgps->lon[idx]);
		lat = refIgps->lat[idx];
		GetIgpInformation(egnosCorr,lat,lon,&(refIgps->bandNumber[idx]),&(refIgps->maskIndex[idx]),&(refIgps->verticalDelay[idx]));
		if((refIgps->bandNumber[idx]!=INVALID_VALUE) && (refIgps->maskIndex[idx]!=INVALID_VALUE))
		{
			refIgps->validElements++;
		}
	}
	if(refIgps->validElements<2)
	{
		/* points at 75° not found, do not continue */
		return;
	}
	/* find the last two points (85°) */
	/* in the band 9 */
	lat = refIgps->lat2;
	ionoMask = &(egnosCorr->ionoMask[altBandNumber]);
	altDelta = 30;
	lon85[0] = altDelta * (int)floor(ippLon/(double)altDelta);
	lon85[1] = lon85[0] + altDelta;
	for(idx=0;idx<2;idx++)
	{
		lon = CorrectLon(lon85[idx]);
		maskIndex = refIgps->maskIndex[idx] = GetMaskIndex(egnosCorr,lat,lon,altBandNumber);
		if(maskIndex!=INVALID_VALUE)
		{
			igpIonoCorr = &(egnosCorr->bandIonoCorrections[altBandNumber].igpIonoCorrections[maskIndex]);
			if((igpIonoCorr->valid==TRUE_FLAG) && (igpIonoCorr->iodi==ionoMask->maskIodi) && (igpIonoCorr->usable==TRUE_FLAG) && (igpIonoCorr->monitored==TRUE_FLAG))
			{
				delay85[idx] = igpIonoCorr->igpVerticalDelay;
				refIgps->validElements++;
			}
		}
	}
	if(refIgps->validElements==4)
	{
		refIgps->bandNumber[0] = refIgps->bandNumber[1] = altBandNumber;
		coeff = (delay85[1]-delay85[0])/(double)altDelta;
		refIgps->verticalDelay[0] = delay85[0] + (coeff * (refIgps->lon[0]-lon85[0]));
		refIgps->verticalDelay[1] = delay85[0] + (coeff * (refIgps->lon[1]-lon85[0]));
		return;
	}
	/* look in the other bands */
	refIgps->validElements=2;
	lat = refIgps->lat2;
	altDelta = 90;
	lon85[0] = altDelta * (int)floor(ippLon/(double)altDelta);
	lon85[1] = lon85[0] + altDelta;
	for(idx=0;idx<2;idx++)
	{
		lon = CorrectLon(lon85[idx]);
		GetIgpInformation(egnosCorr,lat,lon,&(refIgps->bandNumber[idx]),&(refIgps->maskIndex[idx]),&(delay85[idx]));
		if((refIgps->bandNumber[idx]!=INVALID_VALUE) && (refIgps->maskIndex[idx]!=INVALID_VALUE) && (refIgps->bandNumber[idx]!=altBandNumber))
		{
			refIgps->validElements++;
		}
	}
	if(refIgps->validElements==4)
	{
		coeff = (delay85[1]-delay85[0])/(double)altDelta;
		refIgps->verticalDelay[0] = delay85[0] + (coeff * (refIgps->lon[0]-lon85[0]));
		refIgps->verticalDelay[1] = delay85[0] + (coeff * (refIgps->lon[1]-lon85[0]));
		return;
	}
}


void SelectIgpsCaseCSouth(TEgnosCorrections *egnosCorr,TIgpInfo *refIgps,double ippLon)
{
	int idx;
	int lat;
	int lon;
	const int delta = 10;
	const int altBandNumber = 10;
	int altDelta;
	int lon85[2];
	double delay85[2];
	int maskIndex;
	TEgnosIonoMask *ionoMask;
	TEgnosIonoIgpDelayCorrections *igpIonoCorr;
	double coeff;

	refIgps->lat1 = -85;
	refIgps->lat2 = -75;
	refIgps->lon1 = delta * (int)floor(ippLon/(double)delta);
	refIgps->lon2 = refIgps->lon1 + delta;
	refIgps->validElements = 0;
	refIgps->lat[0] = refIgps->lat[1] = refIgps->lat2;
	refIgps->lat[2] = refIgps->lat[3] = refIgps->lat1;
	refIgps->lon[1] = refIgps->lon[2] = refIgps->lon1;
	refIgps->lon[0] = refIgps->lon[3] = refIgps->lon2;
	/* find the first two points (-75°) */
	for(idx=0;idx<2;idx++)
	{
		lon = CorrectLon(refIgps->lon[idx]);
		lat = refIgps->lat[idx];
		GetIgpInformation(egnosCorr,lat,lon,&(refIgps->bandNumber[idx]),&(refIgps->maskIndex[idx]),&(refIgps->verticalDelay[idx]));
		if((refIgps->bandNumber[idx]!=INVALID_VALUE) && (refIgps->maskIndex[idx]!=INVALID_VALUE))
		{
			refIgps->validElements++;
		}
	}
	if(refIgps->validElements<2)
	{
		/* points at -75° not found, do not continue */
		return;
	}
	/* find the last two points (-85°) */
	/* in the band 10 */
	lat = refIgps->lat1;
	ionoMask = &(egnosCorr->ionoMask[altBandNumber]);
	altDelta = 30;
	lon85[0] = altDelta * (int)floor(ippLon/(double)altDelta);
	lon85[1] = lon85[0] + altDelta;
	for(idx=2;idx<4;idx++)
	{
		lon = CorrectLon(lon85[idx-2]);
		maskIndex = refIgps->maskIndex[idx] = GetMaskIndex(egnosCorr,lat,lon,altBandNumber);
		if(maskIndex!=INVALID_VALUE)
		{
			igpIonoCorr = &(egnosCorr->bandIonoCorrections[altBandNumber].igpIonoCorrections[maskIndex]);
			if((igpIonoCorr->valid==TRUE_FLAG) && (igpIonoCorr->iodi==ionoMask->maskIodi) && (igpIonoCorr->usable==TRUE_FLAG) && (igpIonoCorr->monitored==TRUE_FLAG))
			{
				delay85[idx-2] = igpIonoCorr->igpVerticalDelay;
				refIgps->validElements++;
			}
		}
	}
	if(refIgps->validElements==4)
	{
		refIgps->bandNumber[2] = refIgps->bandNumber[3] = altBandNumber;
		coeff = (delay85[1]-delay85[0])/(double)altDelta;
		refIgps->verticalDelay[2] = delay85[0] + (coeff * (refIgps->lon[2]-lon85[0]));
		refIgps->verticalDelay[3] = delay85[0] + (coeff * (refIgps->lon[3]-lon85[0]));
		return;
	}
	/* look in the other bands */
	refIgps->validElements=2;
	lat = refIgps->lat1;
	altDelta = 90;
	lon85[0] = altDelta * (int)floor(ippLon/(double)altDelta);
	lon85[1] = lon85[0] + altDelta;
	for(idx=2;idx<4;idx++)
	{
		lon = CorrectLon(lon85[idx-2]);
		GetIgpInformation(egnosCorr,lat,lon,&(refIgps->bandNumber[idx]),&(refIgps->maskIndex[idx]),&(delay85[idx-2]));
		if((refIgps->bandNumber[idx]!=INVALID_VALUE) && (refIgps->maskIndex[idx]!=INVALID_VALUE) && (refIgps->bandNumber[idx]!=altBandNumber))
		{
			refIgps->validElements++;
		}
	}
	if(refIgps->validElements==4)
	{
		coeff = (delay85[1]-delay85[0])/(double)altDelta;
		refIgps->verticalDelay[2] = delay85[0] + (coeff * (refIgps->lon[2]-lon85[0]));
		refIgps->verticalDelay[3] = delay85[0] + (coeff * (refIgps->lon[3]-lon85[0]));
		return;
	}
}


void SelectIgpsCaseD(TEgnosCorrections *egnosCorr,TIgpInfo *refIgps,double ippLon)
{
	const int bandNumber = 9;
	int idx;
	int lat;
	int lon;
	TEgnosIonoMask *ionoMask = &(egnosCorr->ionoMask[9]);
	int maskIndex;
	TEgnosIonoIgpDelayCorrections *igpIonoCorr;

	refIgps->lat[0] = refIgps->lat[1] = refIgps->lat[2] = refIgps->lat[3] = 85;
	/* longitude values are W180°, W90°, 0° and E90° */
	lon = 90 * (int)floor(ippLon/90.0);
	refIgps->lon[2]=lon;
	refIgps->lon[3]=lon+90;
	refIgps->lon[0]=lon+180;
	refIgps->lon[1]=lon-90;

	refIgps->validElements = 0;
	for(idx=0;idx<4;idx++)
	{
		lon = CorrectLon(refIgps->lon[idx]);
		lat = refIgps->lat[idx];
		refIgps->bandNumber[idx] = bandNumber;
		maskIndex = refIgps->maskIndex[idx] = GetMaskIndex(egnosCorr,lat,lon,bandNumber);
		if(refIgps->maskIndex[idx]!=INVALID_VALUE)
		{
			igpIonoCorr = &(egnosCorr->bandIonoCorrections[bandNumber].igpIonoCorrections[maskIndex]);
			if((igpIonoCorr->valid==TRUE_FLAG) && (igpIonoCorr->iodi==ionoMask->maskIodi) && (igpIonoCorr->usable==TRUE_FLAG) && (igpIonoCorr->monitored==TRUE_FLAG))
			{
				refIgps->verticalDelay[idx] = igpIonoCorr->igpVerticalDelay;
				refIgps->validElements++;
			}
		}
	}
}


int VerticalDelayInterpolationCaseDCaseE(double ippLat,double ippLon,double fpp,TIgpInfo *refIgps,double *verticalDelayPtr)
{
	double xpp;
	double ypp;
	double w[4];
	double verticalDelay = 0.0;
	int idx;

	if(refIgps->validElements==4)
	{
		ypp = (fabs(ippLat)-85.0)/10.0;
		xpp = (((ippLon-(double)refIgps->lon[2])/90.0)*(1.0-(2.0*ypp))) + ypp;
		w[0] = xpp*ypp;
		w[1] = (1.0-xpp)*ypp;
		w[2] = (1.0-xpp)*(1.0-ypp);
		w[3] = xpp*(1.0-ypp);
		for(idx=0;idx<4;idx++)
		{
			verticalDelay += (w[idx]*refIgps->verticalDelay[idx]);
		}
		verticalDelay *= fpp; /* Sign changes w.r.t. the standard, because this correction is subtracted */
		*verticalDelayPtr = verticalDelay;
		return TRUE_FLAG;
	}

	return FALSE_FLAG;
}


void SelectIgpsCaseE(TEgnosCorrections *egnosCorr,TIgpInfo *refIgps,double ippLon)
{
	const int bandNumber = 10;
	int idx;
	int lat;
	int lon;
	TEgnosIonoMask *ionoMask = &(egnosCorr->ionoMask[bandNumber]);
	int maskIndex;
	TEgnosIonoIgpDelayCorrections *igpIonoCorr;

	refIgps->lat[0] = refIgps->lat[1] = refIgps->lat[2] = refIgps->lat[3] = -85;
	/* longitude values are W140°, W50°, E40° and E130° */
	lon = (90 * (int)floor((ippLon+50)/90.0))-50;
	refIgps->lon[2]=lon;
	refIgps->lon[3]=lon+90;
	refIgps->lon[0]=lon+180;
	refIgps->lon[1]=lon-90;

	refIgps->validElements = 0;
	for(idx=0;idx<4;idx++)
	{
		lon = CorrectLon(refIgps->lon[idx]);
		lat = refIgps->lat[idx];
		refIgps->bandNumber[idx] = bandNumber;
		maskIndex = refIgps->maskIndex[idx] = GetMaskIndex(egnosCorr,lat,lon,bandNumber);
		if(refIgps->maskIndex[idx]!=INVALID_VALUE)
		{
			igpIonoCorr = &(egnosCorr->bandIonoCorrections[bandNumber].igpIonoCorrections[maskIndex]);
			if((igpIonoCorr->valid==TRUE_FLAG) && (igpIonoCorr->iodi==ionoMask->maskIodi) && (igpIonoCorr->usable==TRUE_FLAG) && (igpIonoCorr->monitored==TRUE_FLAG))
			{
				refIgps->verticalDelay[idx] = igpIonoCorr->igpVerticalDelay;
				refIgps->validElements++;
			}
		}
	}
}

void PrintIgpsInfo(TEgnosCorrections *egnosCorr,double ippLat,double ippLon,TIgpInfo *refIgps,int condition,double fpp,double correction)
{
	int igpIdx;

	if(egnosCorr->ionoDebugFile==NULL)
	{
		return;
	}

	fprintf(egnosCorr->ionoDebugFile,"\n");
	fprintf(egnosCorr->ionoDebugFile,"\tIono Pierce Point -- Lat: %6.2fd Lon: %6.2fd\n",ippLat,ippLon);
	switch(condition)
	{
		case 1:
			fprintf(egnosCorr->ionoDebugFile,"\tCase a (|Lat| <= 60)\n");
			break;
		case 2:
			fprintf(egnosCorr->ionoDebugFile,"\tCase b (60 < |Lat| <= 75)\n");
			break;
		case 3:
			fprintf(egnosCorr->ionoDebugFile,"\tCase c (75 < |Lat| <= 85)\n");
			break;
		case 4:
			fprintf(egnosCorr->ionoDebugFile,"\tCase d (Lat > 85)\n");
			break;
		case 5:
			fprintf(egnosCorr->ionoDebugFile,"\tCase e (Lat < -85)\n");
			break;
		default:
			return;
			break;
	}
	for(igpIdx=0;igpIdx<refIgps->validElements;igpIdx++)
	{
		if(condition != 3)
		{
			fprintf(egnosCorr->ionoDebugFile,"\t\t Lat %3d  Lon %3d (band: %2d, idx: %3d) VertDelay: %5.2fm\n",refIgps->lat[igpIdx],refIgps->lon[igpIdx],refIgps->bandNumber[igpIdx],refIgps->maskIndex[igpIdx],refIgps->verticalDelay[igpIdx]);
		}
		else
		{
			fprintf(egnosCorr->ionoDebugFile,"\t\t Lat %3d  Lon %3d (band: NA, idx:  NA) VertDelay: %5.2fm (interpolated)\n",refIgps->lat[igpIdx],refIgps->lon[igpIdx],refIgps->verticalDelay[igpIdx]);
		}
	}
	fprintf(egnosCorr->ionoDebugFile,"\tObliquity factor: %4.1f\n",fpp);
	fprintf(egnosCorr->ionoDebugFile,"\tVertical delay  : %4.1f m\n",correction);
}


int EgnosIonosphericCorrection(TNGeneConfiguration *cfg,TEgnosCorrections *egnosCorr,double ippLat,double ippLon,double fpp,double *ionoCorrection)
{
	TIgpInfo refIgps;
	int returnValue = FALSE_FLAG;

	*ionoCorrection = 0.0;

	/* use of EGNOS is disabled */
	if(cfg->pvtConfiguration.egnosFlag==FALSE_FLAG)
	{
		return FALSE_FLAG;
	}

	/* calculate iono delay depending on the latitude
	 * algorithm described by the standard
	 * RTCA DO-229D, December 13, 2006; Appendix A, page A-34 */
	if(fabs(ippLat)<=60.0)
	{
		/* case a */
		SelectIgps(egnosCorr,&refIgps,ippLat,ippLon,5,5);
		if(refIgps.validElements<3)
		{
			SelectIgps(egnosCorr,&refIgps,ippLat,ippLon,10,10);
			if(refIgps.validElements<3)
			{
				return FALSE_FLAG;
			}
		}
		returnValue = VerticalDelayInterpolation(ippLat,ippLon,fpp,&refIgps,ionoCorrection);
		PrintIgpsInfo(egnosCorr,ippLat,ippLon,&refIgps,1,fpp,*ionoCorrection);
	}
	else if(fabs(ippLat)<=75.0)
	{
		/* case b */
		SelectIgps(egnosCorr,&refIgps,ippLat,ippLon,5,10);
		if(refIgps.validElements<3)
		{
			SelectIgps(egnosCorr,&refIgps,ippLat,ippLon,10,10);
			if(refIgps.validElements<3)
			{
				return FALSE_FLAG;
			}
		}
		returnValue = VerticalDelayInterpolation(ippLat,ippLon,fpp,&refIgps,ionoCorrection);
		PrintIgpsInfo(egnosCorr,ippLat,ippLon,&refIgps,2,fpp,*ionoCorrection);
	}
	else if(fabs(ippLat)<=85.0)
	{
		/* case c */
		if(ippLat>0)
		{
			SelectIgpsCaseCNorth(egnosCorr,&refIgps,ippLon);
		}
		else
		{
			SelectIgpsCaseCSouth(egnosCorr,&refIgps,ippLon);
		}
		if(refIgps.validElements!=4)
		{
			return FALSE_FLAG;
		}
		returnValue = VerticalDelayInterpolation(ippLat,ippLon,fpp,&refIgps,ionoCorrection);
		PrintIgpsInfo(egnosCorr,ippLat,ippLon,&refIgps,3,fpp,*ionoCorrection);
	}
	else if(ippLat>85.0)
	{
		/* case d */
		SelectIgpsCaseD(egnosCorr,&refIgps,ippLon);
		if(refIgps.validElements!=4)
		{
			return FALSE_FLAG;
		}
		returnValue = VerticalDelayInterpolationCaseDCaseE(ippLat,ippLon,fpp,&refIgps,ionoCorrection);
		PrintIgpsInfo(egnosCorr,ippLat,ippLon,&refIgps,4,fpp,*ionoCorrection);
	}
	else if(ippLat<-85.0)
	{
		/* case e */
		SelectIgpsCaseE(egnosCorr,&refIgps,ippLon);
		if(refIgps.validElements!=4)
		{
			return FALSE_FLAG;
		}
		returnValue = VerticalDelayInterpolationCaseDCaseE(ippLat,ippLon,fpp,&refIgps,ionoCorrection);
		PrintIgpsInfo(egnosCorr,ippLat,ippLon,&refIgps,5,fpp,*ionoCorrection);
	}
	return returnValue;
}
