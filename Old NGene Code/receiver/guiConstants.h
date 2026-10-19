/**
 * @file guiConstants.h
 *
 * @author Mario Nicola
 *
 * @date 06/08/2014
 *
 * @version 1.0.0
 *
 * @brief Declares constants and symbols for the GUI client.
 */

#ifndef GUI_CONSTANTS_H_INCLUDED
#define GUI_CONSTANTS_H_INCLUDED

/**
 * @def DEFAULT_GUI_TCP_PORT
 * @brief Default value for the number of the port
 *        used for the TCP connection with the GUI.
 */
#define DEFAULT_GUI_TCP_PORT	4999

/**
 * @def GUI_CLIENT_TIMEOUT
 * @brief How many s the client will wait for server before shutting down connection.
 */
#define GUI_CLIENT_TIMEOUT 5

/**
 * @def DATA_TO_GUI_HEADER_SIZE
 * @brief Size in byte of the header in the data packet
 *        to be sent to the GUI with the receiver results
 */
#define DATA_TO_GUI_HEADER_SIZE		188

/**
 * @def DATA_TO_GUI_CHANNEL_SIZE
 * @brief Size in byte of the data packet
 *        to be sent to the GUI with the receiver results for a single channel
 */
#define DATA_TO_GUI_CHANNEL_SIZE 	104

#endif /* GUI_CONSTANTS_H_INCLUDED */
