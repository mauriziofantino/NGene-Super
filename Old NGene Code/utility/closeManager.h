/**
 * @file closeManager.h
 *
 * @author Mario Nicola
 *
 * @date 07/08/2014
 *
 * @version 1.0.0
 *
 * @brief Declares a function that closes the receiver.
 */

#ifndef CLOSE_MANAGER_H_INCLUDED
#define CLOSE_MANAGER_H_INCLUDED


/**
 * @brief CloseNGene terminates the receiver. If the GUI is used,
 *        CloseNGene waits in order to let the GUI close the TCP connection.
 *
 * @param cfg points to the receiver configuration
 * @param returnValue indicates the return value to be passed to exit function.
 */
void CloseNGene(TNGeneConfiguration *cfg,int returnValue);


#endif /* CLOSE_MANAGER_H_INCLUDED */
