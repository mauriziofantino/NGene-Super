/**
 * @file usbInterface.h
 *
 * @author Mario Nicola
 *
 * @date 22/09/2014
 *
 * @version 1.0.0
 *
 * @brief Managing of the USB Interface
 */

#ifndef __USB_INTERFACE_H__
#define __USB_INTERFACE_H__

#include "frontend.h"


/**
 * @def USB_BULK_TRANSFER_TIMEOUT
 * @brief Timeout (ms) for the libusb_fill_bulk_transfer function
 */
#define USB_BULK_TRANSFER_TIMEOUT		5000

/**
 * @def USB_BULK_TRANSFER_TIMEOUT
 * @brief Timeout (ms) for the libusb_control_transfer function
 */
#define USB_CONTROL_TRANSFER_TIMEOUT	1000


/**
 * @brief Open the USB Interface
 * 
 * @param cfg points to the receiver configuration
 * @param rxFe pointer to the Front End structure
 * 
 * @return 0 when everything is ok 1 on error
 */
int OpenUsbFrontend(TNGeneConfiguration * cfg, TRxFe * rxFe);

/**
 * @brief Close the USB Interface
 * 
 * @param rxFe pointer to the Front End structure
 * 
 * @return 0 when everything is ok 1 on error
 */
int CloseUsbFrontend(TRxFe * rxFe);

/**
 * @brief Start the transfer from the USB Interface
 * 
 * @param rxFe pointer to the Front End structure
 * 
 * @return 0 when everything is ok 1 on error
 */
int StartUsbInterface(TRxFe * rxFe);

/**
 * @brief Stop the transfer from the USB Interface
 * 
 * @param rxFe pointer to the Front End structure
 * 
 * @return 0 when everything is ok 1 on error
 */
int StopUsbInterface(TRxFe * rxFe);


/**
 * @brief Read data from the USB buffer into the USB intermediate buffer
 *
 * @param rxFe pointer to the Front End structure
 */
void CopySamplesIntoUsbIntermediateBuffer(TRxFe * rxFe);

/**
 * @brief Read data from the USB Intermediate buffer to the circular buffer
 * 
 * @param rxFe pointer to the Front End structure
 * @param buffer input buffer
 * @param dim how many sample to read
 * 
 * @return number of read bytes
 */
int ReadUsbInterface(TRxFe * rxFe, char *buffer, int dim);

/**
 * @brief Check if there is a USB Buffer overrun
 * 
 * @param rx_fe pointer to the Front End structure
 * 
 * @return 0 when everything is ok 1 on error
 */
int UsbCheckOverrun(TRxFe * rxFe);

#endif
