/**
 * @file    neqUtils.h
 *
 * @author  Paul Whitehall - Nottingham Scientific Ltd
 *
 * @date    23/3/2010
 *
 * @version 1.0.0
 *
 * @brief   This module contains the NSL implementation of the NeQuick model's utility functions.
 *
 *          From the NeQuick model reference code with minor changes.
 */
#ifndef NEQ_UTILS_H_INCLUDED
#define NEQ_UTILS_H_INCLUDED
#include "nequickDataStructures.h"

/**
 * @brief This function is a clipped exponential function. It always returns a valid
 *        double.
 *
 * @param dPower Power for exponential (I)
 *
 * @return the clipped exponential value
 */
double NeqClipExp(double dPower);

/**
 * @brief  This function calculates:
 *
 *                 (dAlpha * dX)
 *         (dF1 * e             ) + dF2
 *         ----------------------------
 *               (dAlpha * dX)
 *              e              + 1
 *
 *   where the exponential terms are clipped to valid values by using
 *   NeqClipExp.
 *
 * @param dF1    A term in the exponential shown above (I)
 * @param dF2    A term in the exponential shown above (I)
 * @param dAlpha A term in the exponential shown above (I)
 * @param dX     A term in the exponential shown above (I)
 *
 * @return The value of the exponential above
 */
double NeqJoin(double dF1, double dF2, double dAlpha, double dX);

/**
 * @brief This function calculates the square of a number.
 *
 * @param dValue Value to be squared (I)
 *
 * @return The square of the value
 */
double NeqSquared(double dValue);
#endif /* NEQ_UTILS_H_INCLUDED */
