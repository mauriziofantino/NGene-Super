/**
 * @file frontend.h
 *
 * @author Mario Nicola
 *
 * @date 16/08/2014
 *
 * @version 1.0.1
 *
 * @brief Managing the Front End
 * @details Derived from a previous version by Nordio, Tomatis, Fantino
 */

#ifndef __RX_FRONT_END_H__INCLUDED
#define __RX_FRONT_END_H__INCLUDED

#include "ngeneConfiguration.h"
#include <libusb-1.0/libusb.h>

/* Set some limitations for the sampling frequency */

/**
 * @def LOW_FS
 * @brief With a sampling frequency under this minimum value,
 *        some simplifications in the generation of the local spreading codes
 *        for the tracking do not hold. [Hz].
 *        Empirically set taking as reference the fs of an Acorde FE (4.096 MHz)
 *        for which the aforementioned simplification was not working.
 */
#define LOW_FS	5.0e6

/**
 * @def MINIMUM_FS_FOR_GPS
 * @brief At least 1 sample for chip: under this minimum,
 *        GPS and EGNOS signals are not elaborated.
 */
#define MINIMUM_FS_FOR_GPS 	1.023e6

/**
 * @def MINIMUM_FS_FOR_GAL
 * @brief At least 1 sample for BOC slot: under this minimum,
 *        Galileo signals are not elaborated.
 */
#define MINIMUM_FS_FOR_GAL 	2.046e6

/* USB parameters */

/**
 * @def USB_BLOCKSIZE_DEFAULT
 * @brief The Default value for the USB_BLOCKSIZE parameter
 *        read from the Front_End.conf configuration file.
 *
 *        Default value from the NVS Stereo Grabber: 1<<14 (not working).
 */
#define USB_BLOCKSIZE_DEFAULT 	(1 << 13)

/**
 * @def USB_BUFFER_SIZE
 * @brief Number of blocks in the usbBuffer array.
 *        Each block is made of USB_BLOCKSIZE bytes, i.e. one USB transfer.
 *        Default value from the NVS Stereo Grabber: 256.
 */
#define USB_BUFFER_SIZE		128

/**
 * @def USB_INTERMEDIATE_BUFFER_SIZE
 * @brief Number of blocks in the usbIntermediateBuffer array.
 * 		  Each block is made of USB_BLOCKSIZE bytes, i.e. one USB transfer.
 *        Default value from the NVS Stereo Grabber: 4096.
 */
#define USB_INTERMEDIATE_BUFFER_SIZE 	512

/**
 * @enum UsbTransferStatusEnum
 * @brief Represents the status of a data transfer or the status of the USB
 */
typedef enum UsbTransferStatusEnum {
    /* these values are equal to enum libusb_transfer_status in libusb.h */
    NGENE_USB_TRANSFER_COMPLETED = 0,			/**<  */
    NGENE_USB_TRANSFER_ERROR = 1,			/**< Transfer failed */
    NGENE_USB_TRANSFER_TIMED_OUT = 2,			/**< Transfer timed out */
    NGENE_USB_TRANSFER_CANCELLED = 3,			/**< Transfer was cancelled */
    NGENE_USB_TRANSFER_STALL = 4,			/**< For bulk/interrupt endpoints: halt condition detected (endpoint
	 	 	 	 	 	 	 	 	 	 	 * stalled). For control endpoints: control request not supported. */
    NGENE_USB_TRANSFER_NO_DEVICE = 5,			/**< Device was disconnected */
    NGENE_USB_TRANSFER_OVERFLOW = 6,			/**< Device sent more data than requested */
    /* additional error codes */
    NGENE_USB_INTERMEDIATE_BUFFER_FULL = 7,	/**< All the data blocks in the intermediate buffer ore busy */
    NGENE_USB_SUBMIT_TRANSFER_ERROR = 8,		/**< An error occurred during the execution of to libusb_submit_transfer */
    /* additional transfer status values */
    NGENE_USB_TRANSFER_OFF = 9,				/**< The transfer has not been started */
    NGENE_USB_TRANSFER_PENDING = 10,			/**< The transfer has been initialized */
    /* default value for USB status variable */
    NGENE_USB_OK = 11,							/**<  */
     /**/ NGENE_USB_OTHER_ERRORS = 12		/**<  */
} TUsbTransferStatus;

/**
 * @struct UsbTransferElementStruct
 * @brief This structure contains variable used to manage one asynchronous transfers.
 */
typedef struct UsbTransferElementStruct {
    char *destinationBuffer;				/**< pointer to the transfer destination */
    struct libusb_transfer *transfer;		/**< libusb data structure */
    TUsbTransferStatus status;				/**< status of the data transfer */
} TUsbTransferElement;

/**
 * @struct UsbEndpointHandleStruct
 * @brief This structure contains information used to manage the current endpoint.
 */
typedef struct UsbEndpointHandleStruct {
    libusb_device_handle *usbParentDeviceHandler;		/**< The device handler */
    int endpoint;										/**< Endpoint for the selected front-end */
    TUsbTransferElement transferArray[USB_BUFFER_SIZE];		/**< The transfer elements array */
    int flagStarted;										/**< To check whether the EP input pipe is started */
} TUsbEndpointHandle;

/**
 * @struct UsbFeStruct
 * @brief Structure which contains variables used to access USB front-end.
 */
typedef struct UsbFeStruct {
    /* sample management */
    char *usbBuffer;							/**< buffer used to receive samples from asynchronous data transfers */
    int usbBufferIndex;							/**< index of oldest transfer still to be processed */
    char *usbIntermediateBuffer;			/**< intermediate buffer between usbBuffer and circular buffer */
    unsigned int usbIntermediateBufferWriteIndex;	/**< index of the first block to be overwritten in the intermediate buffer */
    unsigned int usbIntermediateBufferReadIndex;	/**< index of the first block to be read in the in the intermediate buffer */
    unsigned int usbIntermediateBufferSampleIndex;	/**< index of the first sample in the usbIntermediateBufferReadIndex block */
    /* front-end parameters */
    int index;		    /**< index of the FE as reported in the configuration files */
    int vid;				/**< VID parameter of the USB */
    int pid;				/**< PID parameter of the USB */
    int configuration;
    int interface;
    int altInterface;
    int endpoint;
    int vrqtVendorOut;
    int vrqStartXfer;
    int vrqStopXfer;
    int wValue;
    /* transfer variables */
    TUsbEndpointHandle *epHandle;
    TUsbTransferStatus usbStatus;
    int usbSamplesRead;	    /**< TRUE_FLAG when samples have been read from USB, FALSE_FLAG otherwise */
    /* variables used for USB thread */
    /* Mutex and condition variable to handle writing and reading of intermediate buffer */
    pthread_cond_t intermediateBufferFull;	/**< Condition variable indicating that the intermediate buffer is full.
												 When full, the front-end thread wait that it will be emptied by the receiver thread.
	 	 	 	 	 	 	 	 	 	 	 	 Please notice that this condition should never happen: the risk is to lose data from usb stream!!!! */
    pthread_cond_t intermediateBufferEmpty;	/**< Condition variable indicating that the intermediate buffer is empty.
												 When empty, the receiver thread wait that it will be filled by the front-end thread. */
    pthread_mutex_t frontEndMutex;			/**< This mutex protects both intermediateBufferFull and intermediateBufferEmpty condition variables and the flagBufferFull flag */
    /* Mutex and condition variable to handle the interaction between the two threads: the receiver thread and the front-end thread */
    pthread_mutex_t threadsMutexStopReceiver;		/**< This mutex protects the stopReceiverFlag flag (defined in receiver/ngene.c)*/
    pthread_mutex_t threadsMutexStopFrontEnd;		/**< This mutex protects the stopFrontEndFlag flag (defined in receiver/ngene.c)*/
    int flagBufferFull;							/**< Flag indicating that the intermediate buffer is full. It is to avoid this deadlock:
													 both receiver and front-end threads are blocked on a pthread_cond_wait, which can be released by the other one.*/
#ifdef MONITOR_INTERMEDIATE_BUFFER
    FILE *percentageIntBufFullFile;				/**< File where the percentages of the intermediate buffer full are saved */
#endif
} TUsbFe;

/* Front-end data structure */

/**
 * @struct RxFeStruct
 * @brief groups the variable used to use the Receiver Front End
 */
typedef struct RxFeStruct {
    int inputMethod;	    /**< copy of the variable in the NGene configuration */
    int feInitialized;		/**< if != 0, indicates that Front-end has been initialized */
    FILE *incomingFile;		/**< Pointer to the incoming file, NULL if sample are read from USB */
    TUsbFe usbFe;		/**< to handle samples from the USB */
} TRxFe;

/* Functions */

/**
 * @brief InitializeRxFrontEnd performs suitable initialization on required FE
 *
 * @details If N-Gene works from file, file is opened. If N-Gene works from USB FE,
 * then start of the required FE is performed
 *
 * @param cfg points to the receiver configuration
 * @param rxFe pointer to the Front End structure
 */
int InitializeRxFrontEnd(TNGeneConfiguration * cfg, TRxFe * rxFe);

/**
 * @brief Perform the Front End de-initialization
 *
 * @param cfg points to the receiver configuration
 * @param rxFe pointer to the Front End structure
 *
 * @return 0 when everything is ok 1 on error
 */
int DeinitializeRxFrontEnd(TNGeneConfiguration * cfg, TRxFe * rxFe);

/**
 * @brief Start the Front End
 *
 * @param cfg points to the receiver configuration
 * @param rxFe pointer to the Front End structure
 *
 * @return 0 when everything is ok 1 on error
 */
int StartRxFrontEnd(TNGeneConfiguration * cfg, TRxFe * rxFe);

/**
 * @brief Stop the Front End
 *
 * @param rxFe pointer to the Front End structure
 * @return 0 when everything is ok 1 on error
 */
int StopRxFrontEnd(TRxFe * rxFe);

/**
 * @brief Evaluate if there are problem on the Front End (Buffer Overflow, EOF)
 *
 * @param rxFe pointer to the Front End structure
 *
 * @return 0 when everything is ok 1 on error
 */
int FrontEndCheckStatus(TRxFe * rxFe);

/**
 * @brief Read a new bunch of samples
 *
 * @param rxFe pointer to the Front End structure
 * @param buffer input buffer
 * @param dim how many sample will be read
 *
 * @return The number of read samples
 */
unsigned int ReadRxFrontEnd(TRxFe * rxFe, char *buffer, int dim);

#endif
