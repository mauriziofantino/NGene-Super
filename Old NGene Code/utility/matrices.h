/**
 * @file matrices.h
 *
 * @author Mario Nicola
 *
 * @date 23/10/2014
 *
 * @version 1.0.0
 *
 * @brief Functions used to manage matrices.
 */

#ifndef MATRICES_H_INCLUDED
#define MATRICES_H_INCLUDED

/**
 * @brief Initialize the matrix to zero
 *
 * @param m is the matrix
 * @param rows is the rows number
 * @param cols id the columns number
 */
void MatrixInit(double *m, int rows, int cols);


/**
 * @brief Performs product (rows per columns) between two matrices
 *
 * @param input1 is the pointer to matrix 1
 * @param rows1 number of rows of matrix 1
 * @param columns1 number of columns of matrix 1
 * @param input2 is the pointer to matrix 2
 * @param rows2 number of rows of matrix 2
 * @param columns2 number of columns of matrix 2
 * @param output  = input1 x input2 (return value)
 */
void MatrixProduct(double *input1,int rows1,int columns1,double *input2,int rows2,int columns2,double *output);


/**
 * @brief Performs the matrix transpose
 *
 * @param input is the pointer to matrix
 * @param rows indicates number of rows
 * @param columns indicates number of columns
 * @param output transposed matrix
 */
void MatrixTrasp(double *input, int rows, int columns, double *output);


/**
 * @brief Performs the matrix inversion with the Doolittle method
 *
 * @param input is the pointer to matrix input
 * @param size matrix dimension
 * @param output inverse  matrix  = input^-1 (return value)
 * @param l is the pointer to matrix l
 * @param d is the pointer to matrix d
 * @param u is the pointer to matrix u
 * @param u_t is the pointer to matrix u_t
 * @param il is the pointer to matrix il
 * @param id is the pointer to matrix id
 * @param iu is the pointer to matrix iu
 * @param iu_t is the pointer to matrix iu_t
 * @param iud is the pointer to matrix iud
 * @param det is the matrix determinant
 * @return void
 */
void MatrixInv(double *input, int size, double *output, double *l, double *d, double *u, double *u_t, double *il, double *id, double *iu, double *iu_t, double *iud, double *det);


/**
 * @brief Performs the sum between two matrices
 *
 * @param input1 is the pointer to matrix 1
 * @param rows1 number of rows of matrix 1
 * @param columns1 number of columns of matrix 1
 * @param input2 is the pointer to matrix 2
 * @param rows2 number of rows of matrix 2
 * @param columns2 number of columns of matrix 2
 * @param output  = input1 + input2 (return value)
 */
void MatrixSum(double *input1, int rows1, int columns1, double *input2, int rows2, int columns2, double *output);


/**
 * @brief Performs the difference between two matrices
 *
 * @param input1 is the pointer to matrix 1
 * @param rows1 number of rows of matrix 1
 * @param columns1 number of columns of matrix 1
 * @param input2 is the pointer to matrix 2
 * @param rows2 number of rows of matrix 2
 * @param columns2 number of columns of matrix 2
 * @param output  = input1 - input2 (return value)
 */
void MatrixDiff(double *input1, int rows1, int columns1, double *input2, int rows2, int columns2, double *output);


/**
 * @brief Merges vertically the matrices input1 and input2 in the output matrix output
 *
 * @param input1 is the pointer to matrix 1
 * @param rows1 number of rows of matrix 1
 * @param columns1 number of columns of matrix 1
 * @param input2 is the pointer to matrix 2
 * @param rows2 number of rows of matrix 2
 * @param columns2 number of columns of matrix 2
 * @param output is the output matrix
 */
void MergeMatricesVertically(double *input1, int rows1, int columns1, double *input2, int rows2, int columns2, double *output);

/**
 * @brief Builds an identity matrix
 *
 * @param output is identity matrix (return value)
 * @param size is the matrix dimension
 */
void MatrixIdentity(double *output, int size);


#endif /* MATRICES_H_INCLUDED */
