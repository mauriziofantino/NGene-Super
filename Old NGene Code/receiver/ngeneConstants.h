/**
 * @file ngeneConstants.h
 *
 * @author Mario Nicola
 *
 * @date 06/08/2014
 *
 * @version 1.0.0
 *
 * @brief Declares constants and symbols used thorough the receiver.
 */

#ifndef NGENE_CONSTANTS_H_INCLUDED
#define NGENE_CONSTANTS_H_INCLUDED

/**
 * @def COMPILE_64_BIT
 * @brief In order to compile for a 64 bit with a 64 bit OS,
 *        this symbol should be defined in order to avoid warnings.
 */
#define COMPILE_64_BIT

/**
 * @def TEST_PVT_ACCURACY
 * @brief If defined, the code to perform long tests of PVT accuracy is enabled
 */
//#define TEST_PVT_ACCURACY

/**
 * @def FAKE_CHIMERA_RECEIVER
 * @brief Se definito, il ricevitore non esegue l'elaborazione del segnale a IF ma solo
 *        del messaggio di navigazione Chimera 
 */
//#define FAKE_CHIMERA_RECEIVER

/**
 * @def FAKE_OSNMA_RECEIVER
 * @brief Se definito, il ricevitore non esegue l'elaborazione del segnale a IF ma solo
 *        del messaggio di navigazione
 */
//#define FAKE_OSNMA_RECEIVER

/**
 * @def MONITOR_INTERMEDIATE_BUFFER
 * @brief If enabled,the percentage of the intermediate buffer full is save to file
 */
//#define MONITOR_INTERMEDIATE_BUFFER

/**
 * @def CRITICAL_THRESHOLD
 * @brief If MONITOR_INTERMEDIATE_BUFFER is enabled, CRITICAL_THRESHOLD is used to check if the percentage of intermediate buffer full exceeds it.
 */
#ifdef MONITOR_INTERMEDIATE_BUFFER
#define CRITICAL_THRESHOLD 0
#endif

/* Specific constants are included here */
#include "guiConstants.h"

/* Other receiver constants follow */
/**
 * @def NGENE_VERSION_INDEX
 * @brief It's a string indicating the current release of NGene.
 */
#define NGENE_VERSION_INDEX	"1.0.00"

/**
 * @def NGENE_VERSION_NAME
 * @brief It's a string indicating the name of the current NGene version
 *        using an Ubuntu-like convention (e.g. Utopic Unicorn for Ubuntu 14.10)
 */
#define NGENE_VERSION_NAME 	"NGene"

/**
 * @def GNSS_RX_CFGFOLDER
 * @brief Folder in the user home where configuration and other files.
 */
#define GNSS_RX_CFGFOLDER ".ngene"

/**
 * @def FALSE_FLAG
 * @brief It is the false value when an integer variable is used as a boolean one
 */
#define FALSE_FLAG		0

/**
 * @def TRUE_FLAG
 * @brief It is the true value when an integer variable is used as a boolean one
 */
#define TRUE_FLAG		1

/**
 * @def FAILURE_RETURN
 * @brief Default return value used when a function fails.
 */
#define FAILURE_RETURN    1

/**
 * @def SUCCESS_RETURN
 * @brief Default return value used when a function ends successfully.
 */
#define SUCCESS_RETURN    0

/**
 * @def INVALID_RETURN
 * @brief When a function ends and cannot return FAILURE_RETURN or SUCCESS_RETURN.
 */
#define INVALID_RETURN    -1

/**
 * @def MAX_TAG_LEN
 * @brief Max length for the tag in a configuration file.
 */
#define MAX_TAG_LEN  128

/**
 * @def MAX_ROW_LENGTH
 * @brief Max length for standard string in the receiver.
 */
#define MAX_ROW_LENGTH 512

/**
 * @def MAX_MSG_LENGTH
 * @brief MAX_MSG_LENGTH is maximum length for a text message in the output to GUI
 *                       It must have a value multiple of 4 (due to alignment problems with client)
 */
#define MAX_MSG_LENGTH  64

/**
 * @def PRN_NAME_LENGTH
 * @brief Number of character used to represent a PRN (that is, constellation and index)
 *        It must have a value multiple of 4 (due to alignment problems with client)
 */
#define PRN_NAME_LENGTH 12

/**
 * @def STATUS_LENGTH
 * @brief Number of character used to represent the status of current channel
 *        It must have a value multiple of 4 (due to alignment problems with client)
 */
#define STATUS_LENGTH 	12

/**
 * @def RX_BUFFER_LEN
 * @brief Size of the buffers used to communicate with the GUI
 */
#define RX_BUFFER_LEN 65536

/**
 * @define INVALID_VALUE
 * @brief Used when a variable must be initialized with a invalid value
 * 	      (if the variable should be equal or greater than 0.
 */
#define INVALID_VALUE   (-1)

/**
 * @define POSIX_ALIGNMENT
 * @brief When posix_memalign is called, allocated memory address
 *        must be a multiple of POSIX_ALIGNMENT.
 */
#define POSIX_ALIGNMENT		16

/**
 * @define NUM_XMM_REGISTERS
 * @brief Total number of XMM registers (for SIMD instructions).
 */
#define NUM_XMM_REGISTERS		32

/**
 * @define BITS_PER_BYTE
 * @brief Number of bits in one byte
 */
#define BITS_PER_BYTE 		8

/**
 * @def DEGREE_TO_RADIAN
 * @brief Factor to convert degree to radian \f$ \frac{\pi}{180} \f$
 */
#ifndef DEGREE_TO_RADIAN
#define DEGREE_TO_RADIAN  (0.017453292519943295L)
#endif

/**
 * @def RADIAN_TO_DEGREE
 * @brief Factor to convert radian to degree \f$ \frac{180}{\pi} \f$
 */
#ifndef RADIAN_TO_DEGREE
#define RADIAN_TO_DEGREE  (57.295779513082323L)
#endif

/**
 * @def TWICE_M_PI
 * @brief  2 * M_PI (not defined in math.h!)
 */
#ifndef TWICE_M_PI
#define TWICE_M_PI  (6.283185307179586231996L)
#endif

/**
 * @def INV_2PI
 * @brief \f$ \frac{1}{2\pi} \f$
 */
#ifndef  INV_2PI
#define  INV_2PI  (0.159154943091895345608L)
#endif

/**
 * @def MAX_UINT_DIV_BY_2PI
 * @brief \f$ \frac{UINT_MAX}{2\pi} \f$
 */
#ifndef  MAX_UINT_DIV_BY_2PI
#define  MAX_UINT_DIV_BY_2PI  (683565275.417276740074157714844L)
#endif

#endif /* NGENE_CONSTANTS_H_INCLUDED */
