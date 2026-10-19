/**
 * @file dop.h
 *
 * @author Mario Nicola
 *
 * @date 27/10/2014
 *
 * @version 1.0.0
 *
 * @brief DOP computation
 */


#ifndef DOP_H_INCLUDED
#define DOP_H_INCLUDED

#define GDOP_INDEX_IN_DOP_ARRAY 0
#define PDOP_INDEX_IN_DOP_ARRAY 1
#define HDOP_INDEX_IN_DOP_ARRAY 2
#define VDOP_INDEX_IN_DOP_ARRAY 3
#define TDOP_INDEX_IN_DOP_ARRAY 4

#include "ngeneDataStructure.h"

/**
 * @brief DopEvaluation evaluates the DOP related to the current position
 *        and the constellation used to evaluate the position.
 *
 * @details All needed inputs are evaluated inside this function.
 *          Uses temporary variables allocated for the PVT through the Least Squares method.
 *          The output is saved in the PVT result structure.
 *
 * @param rx is the pointer to the receiver structure
 * @param performedPvt indicates the algorithm used to perform PVT
 *
 * @return TRUE_FLAG if DOPs are evaluated, else FALSE_FLAG
 */
int DopEvaluation(TNGeneData *rx,int performedPvt);

#endif /* DOP_H_INCLUDED */
