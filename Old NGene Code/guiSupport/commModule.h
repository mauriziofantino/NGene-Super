/*
 * @file   CommModule.h
 * @author Intecs
 *
 * @date December 4, 2007, 6:32 PM
 * @version 0.1
 *
 * @brief Communication Module
 * @details This is the Communication Module definition file. It contains
 *          definitions for structures and types and function prototypes.
 *          Functions are made available in order to initialize and finalize
 *          the Module usage, to receive the configuration and
 *          to send output data (including high correlators, ephemeris file
 *          and raw IF samples) via a specified TCP port.
 */

#ifndef _MYTHREADS_H
#define	_MYTHREADS_H

#include "generic.h"
#include "ngeneConfiguration.h"

//////// MN defines for client connectivity //////////////////////////
#define GUI_CLIENT_TIMEOUT 5 /**< how many s the client will wait for server before shutting down connection */


///////////////////////// Public Definitions /////////////////////////

/** Definition of the status that may be assumed by this library. */
enum ComStatus_t { STANDBY, RUNNING, EXITING };

/** Codes of messages exchanged between client and server. */
enum ComMsgCodes_t { ACK,              // Acknowledge message
                     NACK,             // Negative acknowledge message
                     APPLEXIT,         // Notification of client application exit
                     ELABSTART,        // Elaboration start request
                     ELABSTOP,         // Elaboration stop request
                     CONFIG,           // Configuration packet
                     SPECTON,          // Spectrum activation command
                     SPECTOFF,         // Spectrum deactivation command
                     HISTON,           // Histogram activation command
                     HISTOFF,          // Histogram deactivation command
                     DATACHUNK,        // Data packet
                     SPECTRUMCHUNK,    // Spectrum data
                     HISTOGRAMCHUNK,   // histogram data
                     HIGHCORRCHUNK,    // high correlators data packet
                     RAWSAMPLESCHUNK,  // raw IF samples file packet
                     EPHEMERISCHUNK,   // ephemeris data packet
                     FILECHUNK,        // generic file data
                     DUMMYCODE };


///////////////////////// Public Functions /////////////////////////

///// Initialization functions

/**
 * Sets the port on which the server will receive requests from client.
 * This method must be used before ComStart in order to use the correct connection.
 * @param port the port number.
 */
void ComSetPort               (int port);

/**
 * It is used to set the function that will be executed by the Communication
 * module before the start of any elaboration activity.
 */
void ComSetInitFunction       (int (*elab_initialize)(void));

/**
 * It is used to set the function that will be executed by the Communication
 * module after the end of any elaboration activity.
 */
void ComSetTerminateFunction  (int (*elab_terminate)(void));

/**
 * It is used to set the buffer where the configuration parameters have to be
 * stored. It must be used before the call to ComStart().
 * NOTE: this area must be written only by the Communication module and read by
 *       the server application only during the acquisition loop, that is when
 *       the ComGetStatus() returns RUNNING.
 * @param buffer pointer to the pointer that will refer the memory area where
 *        configuration parameters will be stored when received.
 */
void ComSetConfigBuffer(void** buffer);

/**
 * Starts the Communication module. It is used to begin both the thread for
 * data transmission and the thread for command reception.
 */
void ComStart                 (void);

/**
 * It is used to stop the Communication module when the application does not
 * intend to use it again.
 */
void ComStop                  (void);

/**
 * It is used to get the Communication module current status. This function
 * is blocking for the thread that executes it. It returns when the current
 * status is RUNNING or EXITING.
 * @return STANDBY, RUNNING or EXITING.
 */
enum ComStatus_t ComGetStatus (void);

//// Communication functions

/**
 * It is used to send the server output data to the client application.
 * The ComStart() function must have been already called.
 * @param buffer pointer to the buffer that contains the server output data
 *        to be sent.
 * @param length length, in bytes, of the buffer.
 * @return 0 if the function is succesfully executed, a non-zero value otherwise.
 */
int ComSendData               (void *buffer, int length);

/**
 * It is used to send the spectrum data to the client application.
 * The ComSpectrumOn() function must have been already called.
 * @param buffer pointer to the buffer that contains the spectrum data
 *        to be sent.
 * @param length length, in bytes, of the buffer.
 * @return 0 if the function is succesfully executed, a non-zero value otherwise.
 */
int ComSendSpectrum           (void *buffer, int length);

/**
 * It is used to send the histogram data to the client application.
 * The ComHistogramOn() function must have been already called.
 * @param buffer pointer to the buffer that contains the histogram data
 *        to be sent.
 * @param length length, in bytes, of the buffer.
 * @return 0 if the function is succesfully executed, a non-zero value otherwise.
 */
int ComSendHistogram          (void *buffer, int length);

/**
 * It is used to send the ephemeris data to the client application.
 * @param buffer pointer to the buffer that contains the ephemeris data
 *        to be sent.
 * @param length length, in bytes, of the buffer.
 * @return 0 if the function is succesfully executed, a non-zero value otherwise.
 */
int ComSendEphemeris          (void *buffer, int length);

/**
 * It is used to send the high correlators file to the client application.
 * @param pathname name and path of the file containing the high correlators.
 * @return 0 if the send is succesfully completed, a non-zero value otherwise.
 */
int ComSendHighCorrFile       (char *pathname);

/**
 * It is used to send the raw samples file to the client application.
 * @param pathname name and path of the file containing the raw samples.
 * @return 0 if the send is succesfully completed, a non-zero value otherwise.
 */
int ComSendRawSamplesFile       (char *pathname);

/**
 * It is used to get the exclusive control over the memory area where server output
 * data is stored. It must be used at the beginning of the update sections so that
 * the Communication Module will wait until the call to ComUnlockDataArea()
 * before reading and sending it to the client.
 */
void ComLockDataArea          (void);

/**
 * It is used to get the exclusive control over the memory area where spectrum
 * data is stored. It must be used at the beginning of the update sections so that
 * the Communication Module will wait until the call to ComUnlockSpectrumArea()
 * before reading and sending it to the client.
 */
void ComLockSpectrumArea      (void);

/**
 * It is used to get the exclusive control over the memory area where histogram
 * data is stored. It must be used at the beginning of the update sections so that
 * the Communication Module will wait until the call to ComUnlockHistogramArea()
 * before reading and sending it to the client.
 */
void ComLockHistogramArea     (void);

/**
 * It is used to get the exclusive control over the memory area where ephemeris
 * data is stored. It must be used at the beginning of the update sections so that
 * the Communication Module will wait until the call to ComUnlockEphemerisArea()
 * before reading and sending it to the client.
 */
void ComLockEphemerisArea     (void);

/**
 * It is used to release the exclusive control over the memory area where server
 * output data is stored. It must be used at the end of the updated sections so
 * that the Communication Module will be able to access and send it to client
 * application.
 */
void ComUnlockDataArea        (void);

/**
 * It is used to release the exclusive control over the memory area where
 * spectrum data is stored. It must be used at the end of the updated sections so
 * that the Communication Module will be able to access and send it to client
 * application.
 */
void ComUnlockSpectrumArea    (void);

/**
 * It is used to release the exclusive control over the memory area where
 * histogram data is stored. It must be used at the end of the updated sections so
 * that the Communication Module will be able to access and send it to client
 * application.
 */
void ComUnlockHistogramArea   (void);

/**
 * It is used to release the exclusive control over the memory area where ephemeris
 * data is stored. It must be used at the end of the updated sections so
 * that the Communication Module will be able to access and send it to client
 * application.
 */
void ComUnlockEphemerisArea        (void);

/**
 * It is used to know whether the spectrum has been requested by the client
 * application and hence needs to be sent.
 * @return true iff the spectrum has been requested.
 */
boolean ComSpectrumOn         (void);

/**
 * It is used to know whether the histogram has been requested by the client
 * application and hence needs to be sent.
 * @return true iff the spectrum has been requested.
 */
boolean ComHistogramOn        (void);

#endif	/* _MYTHREADS_H */

