/**
 * @file mathFunctions.c
 *
 * @author Maurizio Fantino
 *
 * @date 15/06/2007
 *
 * @version 1.0.0
 *
 * @brief Implementations of basic statistical functions like mean, variance estimation on a full
 *        block of data or by mean of an average window
 *        Threshold evaluation to declare present or absent a signal. It is determined assuming a level of
 *        confidence in terms of False Alarm Probability on the single search space bin.
 *        Mathematical library which extend the ANSI C library math.h.
 *
 * @note It has been designed just to operate with double data
 * 		 It includes stat_functions.c, falsealarm.c and math_functions.c from old NGene implementation.
 */
#include <math.h>
#include <stdlib.h>
#include "mathFunctions.h"

/* FIR */
int FirInit(TFir *fir,int nb, double *b)
{
	int idx;

	fir->nb = nb;
	fir->b = (double *)malloc(fir->nb*sizeof(double));
	fir->nz = fir->nb-1;
	fir->z = (double *)malloc(fir->nz*sizeof(double));

	if((fir->b==NULL) || (fir->z==NULL))
	{
		return EXIT_FAILURE;
	}

	for(idx=0;idx<fir->nb;idx++)
	{
		fir->b[idx] = b[idx];
	}
	FirRestart(fir);
	return EXIT_SUCCESS;
}

void FirSetInitialState(TFir *fir,int nz,double *z)
{
	int idx;

	for(idx=0;(idx<nz)&&(idx<fir->nz);idx++)
	{
		fir->z[idx] = z[idx];
	}
	for(;idx<fir->nz;idx++)
	{
		fir->z[idx] = 0.0;
	}
}

void FirDeinit(TFir *fir)
{
	free(fir->b);
	free(fir->z);
}

void FirRestart(TFir *fir)
{
	int idx;

	for(idx=0;idx<fir->nz;idx++)
	{
		fir->z[idx] = 0.0;
	}
}

double FirPerform(TFir *fir,double xin)
{
	double result = 0.0;
	int idx;

	result = fir->z[0] + (fir->b[0]*xin);
	for(idx=1;idx<fir->nz;idx++)
	{
		fir->z[idx-1] =  fir->z[idx] + (fir->b[idx]*xin);
	}
	fir->z[idx-1] = fir->b[idx]*xin;

	return result;
}

void FirPerformArray(TFir *fir,int nx,double *xin,double *yout)
{
	int idx;

	for(idx=0;idx<nx;idx++)
	{
		yout[idx] = FirPerform(fir,xin[idx]);
	}
}



/* IIR */
int IirInit(TIir *iir,int nb,double *b,int na,double *a)
{
	int idx;

	if(nb>na)
	{
		iir->n = nb;
	}
	else
	{
		iir->n = na;
	}
	iir->b = (double *)malloc(iir->n*sizeof(double));
	iir->a = (double *)malloc(iir->n*sizeof(double));
	iir->nz = iir->n-1;
	iir->z = (double *)malloc(iir->nz*sizeof(double));

	if((iir->b==NULL) || (iir->a==NULL) || (iir->z==NULL))
	{
		return EXIT_FAILURE;
	}

	for(idx=0;(idx<iir->n)&&(idx<nb);idx++)
	{
		iir->b[idx] = b[idx]/a[0];
	}
	for(;idx<iir->n;idx++)
	{
		iir->b[idx] = 0.0;
	}
	for(idx=0;(idx<iir->n)&&(idx<na);idx++)
	{
		iir->a[idx] = a[idx]/a[0];
	}
	for(;idx<iir->n;idx++)
	{
		iir->a[idx] = 0.0;
	}
	IirRestart(iir);
	return EXIT_SUCCESS;
}

void IirSetInitialState(TIir *iir,int nz,double *z)
{
	int idx;

	for(idx=0;(idx<nz)&&(idx<iir->nz);idx++)
	{
		iir->z[idx] = z[idx];
	}
	for(;idx<iir->nz;idx++)
	{
		iir->z[idx] = 0.0;
	}
}

void IirDeinit(TIir *iir)
{
	free(iir->b);
	free(iir->a);
	free(iir->z);
}

void IirRestart(TIir *iir)
{
	int idx;

	for(idx=0;idx<iir->nz;idx++)
	{
		iir->z[idx] = 0.0;
	}
}

double IirPerform(TIir *iir,double xin)
{
	double result = 0.0;
	int idx;

	result = iir->z[0] + (iir->b[0]*xin);
	for(idx=1;idx<iir->nz;idx++)
	{
		iir->z[idx-1] =  iir->z[idx] + (iir->b[idx]*xin) - (iir->a[idx]*result);
	}
	iir->z[idx-1] = (iir->b[idx]*xin) - (iir->a[idx]*result);

	return result;
}

void IirPerformArray(TIir *iir,int nx,double *xin,double *yout)
{
	int idx;

	for(idx=0;idx<nx;idx++)
	{
		yout[idx] = IirPerform(iir,xin[idx]);
	}
}



/* AVERAGE WINDOW */
void InitAverageWindow(TAverageWindow *a, int size, double value)
{
	a->coeff1 = 1 / (double) size;
	a->coeff2 = 1 - a->coeff1;
	a->windowSize = size;
	a->filterValue = value;
}

double GetAverageWindowValue(TAverageWindow *a)
{
    return (a->filterValue);
}

double PerformAverageWindow(TAverageWindow *a, double sample)
{
    a->filterValue = a->coeff1 * sample + a->coeff2 * a->filterValue;
    return (a->filterValue);
}


/* OTHER FUNCTIONS */
unsigned Fact(unsigned n)
{
    if(n == 0)
    	return 1;
    else
    	return (n * Fact(n - 1));
}


/* Equation to get the Threshold
	eqn_arg[0] is the False Alarm Probability
	eqn_arg[1] is the Rayleigh distribution variance
 	eqn_arg[2] is the number of Non-Coherent integrated periods */
double FalseAlarm(double x, double *eqn_arg)
{
    double sum = 0;
    unsigned int i = 0;

    for(i = 0; i < (unsigned int) eqn_arg[2]; i++)
    	sum += pow(x / (2.0 * eqn_arg[1]), (double) i) / (double) (Fact(i));
    sum = sum * exp(-x / (2.0 * eqn_arg[1])) - eqn_arg[0];
    return sum;
}


void Edges(double *xa, double *xb, double (*f) (double, double *), double *eqn_arg)
{
    /* Set the starting interval */
    *xb = *xa + 3.0 * eqn_arg[1];
    /* Search for a crossing point */
    while (f(*xa, eqn_arg) * f(*xb, eqn_arg) > 0.0)
    {
    	*xa = *xb;
    	*xb = *xa + 3.0 * eqn_arg[1];
    }
}


double SolveEqn(double x0, double x2, double (*f) (double, double *), double *eqn_arg, double eps, unsigned int imax, unsigned int *error)
{
    unsigned int i;
    double y0, y1, y2, b, c, temp, y10, y20, y21, xm, ym;
    double x1 = 0.0;
    double xmlast = x0;

    *error = EQN_ERROR_NONE;

    /* Evaluation of the function for the lower x bound; if it is zero I have already found the equation solution */
    y0 = f(x0, eqn_arg);
    if(y0 == 0.0)
    	return x0;
    /* Evaluation of the function for the upper x bound; if it is zero I have found the equation solution */
    y2 = f(x2, eqn_arg);
    if(y2 == 0.0)
    	return x2;
    /* If the multiplication of the two previously evaluation is positive no solution can be found with the iterative method */
    if(y2 * y0 > 0.0)
    {
    	*error = EQN_ERROR_BAD_DATA;
    	return x0;
    }
    /* otherwise I can start searching for it! */
    for(i = 0; i < imax; i++)
    {
    	x1 = 0.5 * (x2 + x0);
    	y1 = f(x1, eqn_arg);

    	/* solution found exactly */
    	if(y1 == 0.0)
    		return x1;

    	/* solution found within the acceptable resolution */
    	if(fabs(x1 - x0) < eps)
    		return x1;

    	if(y1 * y0 > 0.0)
    	{
    		temp = x0;
    		x0 = x2;
    		x2 = temp;
    		temp = y0;
    		y0 = y2;
    		y2 = temp;
    	}
    	y10 = y1 - y0;
    	y21 = y2 - y1;
    	y20 = y2 - y0;
    	if(y2 * y20 < 2.0 * y1 * y10)
    	{
    		x2 = x1;
    		y2 = y1;
    	}
    	else
    	{
    		b = (x1 - x0) / y10;
    		c = (y10 - y21) / (y21 * y20);
    		xm = x0 - b * y0 * (1.0 - c * y1);
    		ym = f(xm, eqn_arg);
    		if(ym == 0.0)
    			return xm;
    		if(fabs(xm - xmlast) < eps)
    			return xm;
    		xmlast = xm;
    		if(ym * y0 < 0.0)
    		{
    			x2 = xm;
    			y2 = ym;
    		}
    		else
    		{
    			x0 = xm;
    			y0 = ym;
    			x2 = x1;
    			y2 = y1;
    		}
    	}
    }
    *error = EQN_ERROR_NO_CONVERGENCE;

    return x1;
}

int NearestInteger(float x)
{
	int y = 0;

	if(x-floorf(x)>=0.5)
		y = (int)floorf(x) + 1;
	else
		y = (int)floorf(x);

	return y;
}

int CeilLog2(int n)
{
	int log2 = 0;
	int twoPow = 1;

	while(twoPow<n)
	{
		log2++;
		twoPow *= 2;
	}
	return log2;
}
