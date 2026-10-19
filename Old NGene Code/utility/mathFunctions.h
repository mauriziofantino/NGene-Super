/**
 * @file mathFunctions.h
 *
 * @author Maurizio Fantino
 * 		   Mario Nicola
 *
 * @date 04/05/2015
 *
 * @version 1.1.0
 *
 * @brief Implementations of basic statistical functions like mean, variance estimation on a full
 *        block of data or by mean of an average window
 *        Threshold evaluation to declare present or absent a signal. It is determined assuming a level of
 *        confidence in terms of False Alarm Probability on the single search space bin.
 *        Mathematical library which extend the ANSI C library math.h.
 *
 * @note It has been designed just to operate with double data
 * 		 It includes stat_functions.h, falsealarm.h and math_functions.h from old NGene implementation.
 * 		 Added FIR and IIR implementation.
 */

#ifndef MATH_FUNCTIONS_H_INCLUDED
#define MATH_FUNCTIONS_H_INCLUDED

/**
 * @struct FirStruct
 *
 * @brief Structure used to implement a FIR filter.
 *        Names and implementation are according to the Matlab convention
 *        (direct form II transposed).
 */
typedef struct FirStruct
{
	int nb;		/**< number of b coefficients */
	double *b;	/**< b coefficients */
	int nz;		/**< length of the delay line */
	double *z;	/**< delay line */
} TFir;


/**
 * @brief Initializes a FIR filter. It allocates memory.
 *
 * @param fir points to the FIR data structure
 * @param nb is the number of taps
 * @param b points to the nb taps
 *
 * @return EXIT_FAILURE if errors occur, EXIT_SUCCESS otherwise
 */
int FirInit(TFir *fir,int nb,double *b);

/**
 * @brief Initializes the current state of a FIR filter.
 *
 * @details It takes into account the current size of the filter
 *          pointed by fir: if nz > fir->nz uses only the first input values;
 *          if nz < fir->nz performs a zero padding.
 *
 * @param fir points to the FIR data structure
 * @param nz is the number of input values
 * @param z points to the nz input values
 *
 * @return EXIT_FAILURE if errors occur, EXIT_SUCCESS otherwise
 */
void FirSetInitialState(TFir *fir,int nz,double *z);

/**
 * @brief Deallocates memory used by FIR filter.
 *
 * @param fir points to the FIR data structure
 */
void FirDeinit(TFir *fir);

/**
 * @brief Cleans the FIR memory.
 *
 * @param fir points to the FIR data structure
 */
void FirRestart(TFir *fir);

/**
 * @brief Performs FIR filter (single sample input)
 *
 * @param fir points to the FIR data structure
 * @param xin is the input (one sample)
 *
 * @return the filter output
 */
double FirPerform(TFir *fir,double xin);

/**
 * @brief Performs FIR filter (array of samples input)
 *
 * @param fir points to the FIR data structure
 * @param nx number of input samples
 * @param xin points to the input array
 * @param yout points to the output array
 */
void FirPerformArray(TFir *fir,int nx,double *xin,double *yout);




/**
 * @struct IirStruct
 *
 * @brief Structure used to implement a IIR filter.
 *        Names and implementation are according to the Matlab convention.
 *        (direct form II transposed).
 *
 * @details if the number of a coefficients differs from the number of b coefficients,
 *          a zero padding is performed on the shorter array of coefficients.
 */
typedef struct IirStruct
{
	int n;		/**< number of coefficients */
	double *a;  /**< a coefficients*/
	double *b;	/**< b coefficients */
	int nz;		/**< size of the delay line */
	double *z;	/**< length of the delay line */
} TIir;


/**
 * @brief Initializes a IIR filter. It allocates memory.
 *
 * @param iir points to the IIR data structure
 * @param nb is the number of b taps
 * @param b points to the nb taps
 * @param na is the number of a taps
 * @param a points to the na taps
 *
 * @return EXIT_FAILURE if errors occur, EXIT_SUCCESS otherwise
 */
int IirInit(TIir *iir,int nb,double *b,int na,double *a);

/**
 * @brief Initializes the current state of an IIR filter.
 *
 * @details It takes into account the current size of the filter
 *          pointed by fir: if nz > fir->nz uses only the first input values;
 *          if nz < fir->nz performs a zero padding.
 *
 * @param fir points to the IIR data structure
 * @param nz is the number of input values
 * @param z points to the nz input values
 *
 * @return EXIT_FAILURE if errors occur, EXIT_SUCCESS otherwise
 */
void IirSetInitialState(TIir *iir,int nz,double *z);

/**
 * @brief Deallocates memory used by IIR filter.
 *
 * @param iir points to the IIR data structure
 */
void IirDeinit(TIir *iir);

/**
 * @brief Cleans the IIR memory.
 *
 * @param fir points to the IIR data structure
 */
void IirRestart(TIir *iir);

/**
 * @brief Performs IIR filter (single sample input)
 *
 * @param iir points to the IIR data structure
 * @param xin is the input (one sample)
 *
 * @return the filter output
 */
double IirPerform(TIir *iir,double xin);

/**
 * @brief Performs IIR filter (array of samples input)
 *
 * @param iir points to the IIR data structure
 * @param nx number of input samples
 * @param xin points to the input array
 * @param yout points to the output array
 */
void IirPerformArray(TIir *iir,int nx,double *xin,double *yout);




/**
 * @struct AverageWindowStruct
 *
 * @brief Used to emulate a Hatch filter with programmable weight
 *        (approximates an average)
 */
typedef struct AverageWindowStruct
{
    int windowSize;			/**< size of the window */
    double coeff1, coeff2;	/**< filter coefficients */
    double filterValue;		/**< current averaged value */
} TAverageWindow;

/**
 * @brief Function to initialize the filter
 *
 * @param a is the pointer to the AverageWindowStruct structure
 * @param size is the window length in term of considered samples
 * @param value to be stored as filter memory
 */
void InitAverageWindow(TAverageWindow *a, int size, double value);

/**
 * @brief Function which gets the current filter value (just a wrapper function)
 *
 * @param a is the pointer to the AverageWindowStruct structure
 *
 * @return the current stored filter value
 */
double GetAverageWindowValue(TAverageWindow *a);

/**
 * @brief Function which performs a smoothing filter operation on a block of data following the
 * recursive operation implemented is: \f$ y[n]=\frac{1}{N}x[n]+\frac{N-1}{N}\cdot y[n-1]\f$
 *
 * @param a is the pointer to the AverageWindowStruct structure
 * @param sample is the current fresh data to smooth
 *
 * @return the filter output (it is the same value catch by GetFilterValue)
 */
double PerformAverageWindow(TAverageWindow *a, double sample);

/**
 * @brief Function to determine the factorial of an integer number
 * @param n is the integer number
 * @return \f$ n!\f$
 */
unsigned Fact(unsigned n);

/**
 * @brief Formula of the False Alarm Probability
 * @details The probability density function at the envelope detector input, in misalignment condition or in
 *          absence of signal is given by a Rayleight distribution. The relationship which couples the false
 *          alarm probability and the acquisition threshold is given by equation:
 *          \f$ P_{fa}(V_t^2)=e^{-\frac{Vt^2}{2\sigma}}\displaystyle\sum_{i=0}^{K-1}\frac{1}{i!}\left(\frac{V_t^2}{2\sigma^2}\right)^i \f$
 *
 *          where
 *          - \f$ V_t^2\f$ is the squarred value of the threshold
 *          - K is the number of Non-Coherent integration periods
 *          - \f$\sigma^2\f$ is the Rayleight variance
 *
 * @param x is the function argument
 * @param eqn_arg is a vector which must contain respectively:
 *        - the desired False Alarm probability,
 *        - the variance of the Rayleight distribution at the input of the envelope detector,
 *        - the number of Non-Coherent integrations
 * @return the false alarm probability \f$ P_{fa}\f$ evaluated for the threshold \f$ V_t^2\f$
 */
double FalseAlarm(double x, double *eqn_arg);


/**
 * @brief Formula to get the edges where a valid solution for the function threshold can be found. The step is
 *        set equal to 3 times the variance of the rayleigh distribution estimated at the envelope detector input
 * @param xa is the lower estimated function argument limit
 * @param xb is the upper estimated function argument limit
 * @param f is the pointer to the threshold function which is need to found the range
 * @param eqn_arg is the pointer to the extra argument of the threshold function (see the function threshold)
 */
void Edges(double *xa, double *xb, double (*f) (double, double *), double *eqn_arg);

/**
 * @def EQN_ERROR_NONE
 * @brief No solutio
 */
#define EQN_ERROR_NONE 0
/**
 * @def EQN_ERROR_BAD_DATA
 * @brief Bad data
 */
#define EQN_ERROR_BAD_DATA 1
/**
 * @def EQN_ERROR_NO_CONVERGENCE
 * @brief  No algorithm convergence
 */
#define EQN_ERROR_NO_CONVERGENCE 2

/**
 * @brief Solve_eqn is a function for the root finding of non linear equation. It performs an iterative search
 * over a maximum of imax iteration.
 *
 * @param x0 is the lower argument of the function containing a valid solution
 * @param x2 is the upper argument of the function containing a valid solution
 * @param *f is a pointer to the function to solve which takes a standard (x) input argument and two extra
 *        input argument which are a list of integer and a list of double variables
 * @param *eqn_arg argument list for function f
 * @param eps is the root error resolution
 * @param imax is the maximum number of iteration to run
 * @param *error is an error message which can assume the value:
 *        -EQN_ERROR_NONE           [0]
 *        -EQN_ERROR_BAD_DATA       [1]
 *        -EQN_ERROR_NO_CONVERGENCE [2]
 *
 * @return the equation solution
 */
double SolveEqn(double x0, double x2, double (*f) (double, double *), double *eqn_arg, double eps, unsigned int imax, unsigned int *error);


/**
 * @brief Return the integer number nearest to the input floating number
 *
 * @param x is a float number
 *
 * @return The nearest integer number
 */
int NearestInteger(float x);


/**
 * @brief CeilLog2 evaluates the ceiling of the log2 of the input argument
 *
 * @param n is the integer number
 *
 * @return the ceiling of the log2
 */
int CeilLog2(int n);

#endif /* MATH_FUNCTIONS_H_INCLUDED */
