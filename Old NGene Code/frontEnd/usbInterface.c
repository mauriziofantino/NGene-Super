/**
 * @file usbInterface.c
 *
 * @author Mario Nicola
 *
 * @date 22/09/2014
 *
 * @version 1.0.1
 *
 * @brief From a previous implementation by Matteo Sticco
 *
 */

#include <stdlib.h>
#include <libusb-1.0/libusb.h>
#include <pthread.h>
#include "simd.h"
#include "frontend.h"
#include "ioFunctions.h"
#include "closeManager.h"
#include "ngeneConstants.h"
#include "arrays.h"
#include "usbDevicesConstants.h"
#include "usbInterface.h"


/* Global variable */
extern int USB_BLOCKSIZE; /**< Defined in manageUsbFrontEndConfiguration.c */

/* internal functions */
void FindUsbDevice(unsigned short vid, unsigned short pid, libusb_device **ppUsbDev)
{
    libusb_device **list;
    libusb_device *dev = NULL;
    struct libusb_device_descriptor desc;
    int cnt, i;
    int ret;

    if (ppUsbDev == NULL) {
	return;
    }
    (*ppUsbDev) = NULL;

    cnt = libusb_get_device_list(NULL, &list);
    if (cnt < 0) {
	return;
    }

    for (i = 0; i < cnt; i++) {
	dev = list[i];
	ret = libusb_get_device_descriptor(dev, &desc);
	if (ret < 0) {
	    return;
	}
	if ((desc.idVendor == vid) && (desc.idProduct == pid)) {
	    (*ppUsbDev) = dev;
	    return;
	}
    }
}

void OpenUsbDevice(TUsbFe *usbFe, libusb_device *pUsbDev, libusb_device_handle **ppUDH)
{
    int returnValue;

    /* open device handle */
    returnValue = libusb_open(pUsbDev, ppUDH);
    if (returnValue < 0) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error in libusb_open (%d)", returnValue);
	return;
    }
    /* set configuration */
    if ((usbFe->index != NGENE_FE_AMUNGO_NT1065) && (usbFe->index != NGENE_FE_USRP) && (usbFe->index != NGENE_FE_JRC)) {
	returnValue = libusb_set_configuration((*ppUDH), usbFe->configuration);
	if (returnValue < 0) {
	    SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error in libusb_set_configuration (%d)", returnValue);
	    libusb_close(*ppUDH);
	    (*ppUDH) = NULL;
	    return;
	}
    }
    /* claim interface */
    returnValue = libusb_claim_interface((*ppUDH), usbFe->interface);
    if (returnValue < 0) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error in libusb_claim_interface (%d)", returnValue);
	libusb_close(*ppUDH);
	(*ppUDH) = NULL;
	return;
    }
    /* set alternate interface */
    if ((usbFe->index != NGENE_FE_AMUNGO_NT1065) && (usbFe->index != NGENE_FE_USRP) && (usbFe->index != NGENE_FE_JRC)) {
	returnValue = libusb_set_interface_alt_setting(*ppUDH, usbFe->interface, usbFe->altInterface);
	if (returnValue < 0) {
	    SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error in libusb_set_interface_alt_setting (%d)", returnValue);
	    libusb_release_interface(*ppUDH, usbFe->interface);
	    libusb_close(*ppUDH);
	    (*ppUDH) = NULL;
	    return;
	}
    }
}

void CloseUsbDevice(TUsbFe *usbFe, libusb_device_handle *pUDH)
{
    if (pUDH == NULL) {
	return;
    }

    libusb_release_interface(pUDH, usbFe->interface);
    libusb_close(pUDH);
}


void UsbReadCallback(struct libusb_transfer *transfer)
{
    TUsbTransferElement *pTransferElem = (TUsbTransferElement *) (transfer->user_data);

    switch (transfer->status) {
    case LIBUSB_TRANSFER_COMPLETED:
	/* Mark the transfer as complete */
	pTransferElem->status = NGENE_USB_TRANSFER_COMPLETED;
	break;
    case LIBUSB_TRANSFER_TIMED_OUT:
	pTransferElem->status = NGENE_USB_TRANSFER_TIMED_OUT;
	break;
    case LIBUSB_TRANSFER_OVERFLOW:
	pTransferElem->status = NGENE_USB_TRANSFER_OVERFLOW;
	break;
    case LIBUSB_TRANSFER_ERROR:
	pTransferElem->status = NGENE_USB_TRANSFER_ERROR;
	break;
    case LIBUSB_TRANSFER_CANCELLED:
	pTransferElem->status = NGENE_USB_TRANSFER_CANCELLED;
	break;
    case LIBUSB_TRANSFER_STALL:
	pTransferElem->status = NGENE_USB_TRANSFER_STALL;
	break;
    default:
	pTransferElem->status = NGENE_USB_OTHER_ERRORS;
	break;
    }

}


TUsbEndpointHandle *MakeEndpointHandle(TUsbFe *usbFe, libusb_device_handle *usbDevHandle)
{
    TUsbEndpointHandle *pTmpEpHandle;
    uintptr_t blkIdx;

    /* Sanity checks */
    if (!(usbFe->endpoint & 0x80)) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Invalid endpoint in MakeEndpointHandle (%x)", usbFe->endpoint);
	return NULL;
    }
    /* Alloc the endpoint handler signal structure */
    pTmpEpHandle = (TUsbEndpointHandle *) malloc(sizeof(TUsbEndpointHandle));
    if (NULL == pTmpEpHandle) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error from malloc in MakeEndpointHandle");
	return NULL;
    }
    /* Save the USB device handler in the endpoint handler */
    pTmpEpHandle->usbParentDeviceHandler = usbDevHandle;
    /* Save the USB device handler in the endpoint handler */
    pTmpEpHandle->endpoint = usbFe->endpoint;
    /* Transfers array initialization elem by elem */
    for (blkIdx = 0; blkIdx < USB_BUFFER_SIZE; blkIdx++) {
	(pTmpEpHandle->transferArray[blkIdx]).destinationBuffer = &(usbFe->usbBuffer[blkIdx * USB_BLOCKSIZE]);
	(pTmpEpHandle->transferArray[blkIdx]).transfer = libusb_alloc_transfer(0);
	if ((pTmpEpHandle->transferArray[blkIdx]).transfer == NULL) {
	    SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Error from libusb_alloc_transfer in MakeEndpointHandle");
	    return NULL;
	}
	/* Fill the bulk request */
	libusb_fill_bulk_transfer((pTmpEpHandle->transferArray[blkIdx]).transfer, usbDevHandle, usbFe->endpoint, (unsigned char *) ((pTmpEpHandle->transferArray[blkIdx]).destinationBuffer), USB_BLOCKSIZE, UsbReadCallback, (void *) &(pTmpEpHandle->transferArray[blkIdx]), USB_BULK_TRANSFER_TIMEOUT);

	/* Initialize the status at OFF */
	(pTmpEpHandle->transferArray[blkIdx]).status = NGENE_USB_TRANSFER_OFF;
    }

    /*! Not started yet */
    pTmpEpHandle->flagStarted = FALSE_FLAG;

    return pTmpEpHandle;
}


void DeleteEndpointHandle(TUsbEndpointHandle *eph)
{
    int blkIdx;

    if (NULL == eph) {
	/* Error! The EP handler is null! */
	return;
    }

    if (eph->flagStarted == TRUE_FLAG) {
	/* Error! The EP handler looks busy! */
	return;
    }
    for (blkIdx = 0; blkIdx < USB_BUFFER_SIZE; blkIdx++) {
	if (NULL != (eph->transferArray[blkIdx]).transfer) {
	    libusb_free_transfer((eph->transferArray[blkIdx]).transfer);
	}
    }
    free(eph);
}


/* interface functions */


int OpenUsbFrontend(TNGeneConfiguration *cfg, TRxFe *rxFe)
{
    TUsbFe *usbFe = &(rxFe->usbFe);
    libusb_device *pUsbDevice = NULL;
    libusb_device_handle *pUsbDevHandle = NULL;

    usbFe->index = cfg->fePars->index;
    usbFe->vid = cfg->fePars->vid;
    usbFe->pid = cfg->fePars->pid;
    switch (cfg->usedFrontEnd) {
    case NGENE_FE_NFUELS:
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "N-Fuels virtual front-end requires input from file!");
	CloseNGene(cfg, EXIT_FAILURE);
	break;
    case NGENE_FE_STEREO:
	usbFe->configuration = STEREO_CONFIG;
	usbFe->interface = STEREO_INTF;
	usbFe->altInterface = STEREO_ALTINTF;
	usbFe->endpoint = STEREO_RX_EP;
	usbFe->vrqtVendorOut = STEREO_VRQT_VENDOR_OUT;
	usbFe->vrqStartXfer = STEREO_VRQ_START_XFER;
	usbFe->vrqStopXfer = STEREO_VRQ_STOP_XFER;
	usbFe->wValue = STEREO_WVALUE;
	break;
    case NGENE_FE_CUSTOM:
    case NGENE_FE_FRAUNHOFER:
    case NGENE_FE_SIGE:
    case NGENE_FE_SIGE_V3:
	usbFe->configuration = SIGE_CONFIG;
	usbFe->interface = SIGE_INTF;
	usbFe->altInterface = SIGE_ALTINTF;
	usbFe->endpoint = SIGE_RX_EP;
	usbFe->vrqtVendorOut = SIGE_VRQT_VENDOR_OUT;
	usbFe->vrqStartXfer = SIGE_VRQ_START_XFER;
	usbFe->vrqStopXfer = SIGE_VRQ_STOP_XFER;
	usbFe->wValue = SIGE_WVALUE;
	break;
    case NGENE_FE_AMUNGO_NT1065:
	usbFe->configuration = NT1065_CONFIG;
	usbFe->interface = NT1065_INTF;
	usbFe->altInterface = NT1065_ALTINTF;
	usbFe->endpoint = NT1065_RX_EP;
	usbFe->vrqtVendorOut = SIGE_VRQT_VENDOR_OUT;
	usbFe->vrqStartXfer = SIGE_VRQ_START_XFER;
	usbFe->vrqStopXfer = SIGE_VRQ_STOP_XFER;
	usbFe->wValue = SIGE_WVALUE;
	break;
    case NGENE_FE_JRC:
    case NGENE_FE_USRP:
	/* TODO CHECK!!!! */
	usbFe->configuration = USRP_B200_CONFIG;
	usbFe->interface = USRP_B200_INTF;
	usbFe->altInterface = USRP_B200_ALTINTF;
	usbFe->endpoint = USRP_B200_RX_EP;
	usbFe->vrqtVendorOut = SIGE_VRQT_VENDOR_OUT;
	usbFe->vrqStartXfer = SIGE_VRQ_START_XFER;
	usbFe->vrqStopXfer = SIGE_VRQ_STOP_XFER;
	usbFe->wValue = DEFAULT_WVALUE;
	break;
    default:
	usbFe->configuration = DEFAULT_CONFIG;
	usbFe->interface = DEFAULT_RX_INTERFACE;
	usbFe->altInterface = DEFAULT_RX_ALTINTERFACE;
	usbFe->endpoint = DEFAULT_RX_ENDPOINTS;
	usbFe->vrqtVendorOut = DEFAULT_VRT_VENDOR_OUT;
	usbFe->vrqStartXfer = DEFAULT_VRQ_START_XFER;
	usbFe->vrqStopXfer = DEFAULT_VRQ_STOP_XFER;
	usbFe->wValue = DEFAULT_WVALUE;
	break;
    }
     /**/ posix_memalign((void **) &(usbFe->usbBuffer), POSIX_ALIGNMENT, USB_BUFFER_SIZE * USB_BLOCKSIZE * sizeof(char));
    if (usbFe->usbBuffer == NULL) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Malloc returned a NULL pointer in OpenUsbFrontend");
	CloseNGene(cfg, EXIT_FAILURE);
    }
    usbFe->usbBufferIndex = 0;
    posix_memalign((void **) &(usbFe->usbIntermediateBuffer), POSIX_ALIGNMENT, USB_INTERMEDIATE_BUFFER_SIZE * USB_BLOCKSIZE * sizeof(char));
    if (usbFe->usbIntermediateBuffer == NULL) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Malloc returned a NULL pointer in OpenUsbFrontend");
	CloseNGene(cfg, EXIT_FAILURE);
    }
    usbFe->usbIntermediateBufferWriteIndex = 0;
    usbFe->usbIntermediateBufferReadIndex = 0;
    usbFe->usbIntermediateBufferSampleIndex = 0;
     /**/ libusb_init(NULL);
    /* Search for the selected USB device */
    FindUsbDevice(usbFe->vid, usbFe->pid, &pUsbDevice);
    if (pUsbDevice == NULL) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Cannot find selected USB front-end (VID/PID %04x/%04x)", usbFe->vid, usbFe->pid);
	return FAILURE_RETURN;
    }
    /* Now create a handle for the device, and selects interface and alternative configuration */
    OpenUsbDevice(usbFe, pUsbDevice, &pUsbDevHandle);
    if (pUsbDevHandle == NULL) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Cannot create a handle for the selected USB front-end (VID/PID %04x/%04x)", usbFe->vid, usbFe->pid);
	return FAILURE_RETURN;
    }
    /* Allocate the endpoint handler. This will be shared between the library */
    usbFe->epHandle = MakeEndpointHandle(usbFe, pUsbDevHandle);
    if (usbFe->epHandle == NULL) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Cannot initialize asynchronous transfers for the selected USB front-end (VID/PID %04x/%04x)", usbFe->vid, usbFe->pid);
	return FAILURE_RETURN;
    }
    usbFe->usbStatus = NGENE_USB_OK;
    usbFe->usbSamplesRead = FALSE_FLAG;
    return SUCCESS_RETURN;
}


int CloseUsbFrontend(TRxFe *rxFe)
{
    TUsbFe *usbFe = &(rxFe->usbFe);
    libusb_device_handle *pUsbDevHandle = usbFe->epHandle->usbParentDeviceHandler;

    CloseUsbDevice(usbFe, pUsbDevHandle);
    DeleteEndpointHandle(usbFe->epHandle);
    usbFe->epHandle = NULL;
    free(usbFe->usbBuffer);
    free(usbFe->usbIntermediateBuffer);
    libusb_exit(NULL);

    return SUCCESS_RETURN;
}


int StartUsbInterface(TRxFe *rxFe)
{
    TUsbFe *usbFe = &(rxFe->usbFe);
    TUsbEndpointHandle *epHandle = usbFe->epHandle;
    int ret, blkIdx;
    libusb_device_handle *pUsbDevHandle;

    /*! Check if the EP handler is valid */
    if (epHandle == NULL) {
	return FAILURE_RETURN;
    }

    /* Get the USB device handler */
    pUsbDevHandle = epHandle->usbParentDeviceHandler;
    if (NULL == pUsbDevHandle) {
	return FAILURE_RETURN;
    }

    /* Send out all the bulk transfers on the USB bus */
    for (blkIdx = 0; blkIdx < USB_BUFFER_SIZE; blkIdx++) {
	ret = libusb_submit_transfer((epHandle->transferArray[blkIdx]).transfer);
	if (ret < 0) {
	    SwrxLog(NGENE_LOG_SEVERITY_ERROR, "libusb_submit_transfer error (%d) in StartUsbInterface", ret);
	    return FAILURE_RETURN;
	}
	(epHandle->transferArray[blkIdx]).status = NGENE_USB_TRANSFER_PENDING;
    }

    /* Send a "start transfer" message to the Cypress GPIF */
    if ((usbFe->index != NGENE_FE_AMUNGO_NT1065) && (usbFe->index != NGENE_FE_USRP) && (usbFe->index != NGENE_FE_JRC)) {
	ret = libusb_control_transfer(pUsbDevHandle,	/* dev_handle */
				      usbFe->vrqtVendorOut,	/* request_type */
				      usbFe->vrqStartXfer,	/* bRequest */
				      usbFe->wValue,	/* wValue */
				      0,	/* wIndex */
				      NULL,	/* data */
				      0,	/* wLength */
				      USB_CONTROL_TRANSFER_TIMEOUT);	/* timeout */
	if (ret < 0) {
	    SwrxLog(NGENE_LOG_SEVERITY_ERROR, "libusb_control_transfer error (%d) in StartUsbInterface", ret);
	    return FAILURE_RETURN;
	}
    }
    /* Set the EP handler flag to "started" */
    epHandle->flagStarted = TRUE_FLAG;

    return SUCCESS_RETURN;
}


/* TODO Se la chiamata a libusb_control_transfer viene eseguita con il SiGE viene ritornato un valore di errore,
 *      ma tutto sembra funzionare. Indagare? */
int StopUsbInterface(TRxFe *rxFe)
{
    TUsbFe *usbFe = &(rxFe->usbFe);
    TUsbEndpointHandle *epHandle = usbFe->epHandle;
//      int ret;
    int blkIdx;
    libusb_device_handle *pUsbDevHandle = epHandle->usbParentDeviceHandler;

    if (NULL == epHandle) {
	return FAILURE_RETURN;
    }
    if (epHandle->flagStarted == FALSE_FLAG) {
	return SUCCESS_RETURN;
    }
    if (NULL == pUsbDevHandle) {
	return FAILURE_RETURN;
    }

    /* First send a STOP command to the Cypress microcontroller */
//      ret =
    libusb_control_transfer(pUsbDevHandle,	/* dev_handle */
			    usbFe->vrqtVendorOut,	/* request_type */
			    usbFe->vrqStopXfer,	/* bRequest */
			    usbFe->wValue,	/* wValue */
			    0,	/* wIndex */
			    NULL,	/* data */
			    0,	/* wLength */
			    USB_CONTROL_TRANSFER_TIMEOUT);	/* timeout */
//      if(ret<0)
//      {
//              return FAILURE_RETURN;
//      }

    /* Now discard all the pending URBs */
//      ret = 0;
    for (blkIdx = 0; blkIdx < USB_BUFFER_SIZE; blkIdx++) {
	libusb_cancel_transfer(epHandle->transferArray[blkIdx].transfer);
	epHandle->transferArray[blkIdx].status = NGENE_USB_TRANSFER_OFF;
    }

    /* Declare the endpoint handle stopped */
    epHandle->flagStarted = FALSE_FLAG;

    return SUCCESS_RETURN;
}


void CopySamplesIntoUsbIntermediateBuffer(TRxFe *rxFe)
{
    TUsbFe *usbFe = &(rxFe->usbFe);
    TUsbEndpointHandle *epHandle = usbFe->epHandle;
    TUsbTransferElement *currTransfer;
    int idx = 0;
    char *srcPtr;
    char *dstPtr;
    int samplesWritten = FALSE_FLAG;

    libusb_handle_events(NULL);

    pthread_mutex_lock(&usbFe->frontEndMutex);
    currTransfer = &(epHandle->transferArray[usbFe->usbBufferIndex]);
    while ((currTransfer->status == NGENE_USB_TRANSFER_COMPLETED) && (idx < USB_BUFFER_SIZE)) {
	/* copy samples */
	srcPtr = currTransfer->destinationBuffer;
	dstPtr = &(usbFe->usbIntermediateBuffer[usbFe->usbIntermediateBufferWriteIndex * USB_BLOCKSIZE]);
	ArrayCopy(srcPtr, dstPtr, USB_BLOCKSIZE);
	/* new transfer */
	if (libusb_submit_transfer(currTransfer->transfer) < 0) {
	    usbFe->usbStatus = NGENE_USB_SUBMIT_TRANSFER_ERROR;
	}
	currTransfer->status = NGENE_USB_TRANSFER_PENDING;
	/* update indexes */
	idx++;
	usbFe->usbBufferIndex = (usbFe->usbBufferIndex + 1) % USB_BUFFER_SIZE;
	currTransfer = &(epHandle->transferArray[usbFe->usbBufferIndex]);
	usbFe->usbIntermediateBufferWriteIndex = (usbFe->usbIntermediateBufferWriteIndex + 1) % USB_INTERMEDIATE_BUFFER_SIZE;
	samplesWritten = TRUE_FLAG;
	usbFe->usbSamplesRead = TRUE_FLAG;
	//
	if (usbFe->usbIntermediateBufferWriteIndex == usbFe->usbIntermediateBufferReadIndex) {
	    /* intermediate buffer is full */
	    // do not stop NGene2 when the intermediate buffer is full
	    // usbFe->usbStatus = NGENE_USB_INTERMEDIATE_BUFFER_FULL;
	    usbFe->flagBufferFull = TRUE_FLAG;
	    pthread_cond_signal(&rxFe->usbFe.intermediateBufferEmpty);
	    while (usbFe->usbIntermediateBufferWriteIndex == usbFe->usbIntermediateBufferReadIndex) {
		pthread_cond_wait(&usbFe->intermediateBufferFull, &usbFe->frontEndMutex);
		// if a cond_signal is received, but the buffer is still full, repeat the cond_wait
	    }
	}
    }
    /* verify errors */
    if ((currTransfer->status == NGENE_USB_TRANSFER_TIMED_OUT) && (usbFe->usbSamplesRead == TRUE_FLAG)) {
	// if timeout and some samples have been read, try to restart
	// if no samples have been read, better to stop and try to change the USB_BLOCKSIZE
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "LIBUSB_TRANSFER_TIMED_OUT: NGene2 restarts USB...");
	StopUsbInterface(rxFe);
	StartUsbInterface(rxFe);
	usbFe->usbIntermediateBufferWriteIndex = 0;
	usbFe->usbIntermediateBufferReadIndex = 0;
	usbFe->usbIntermediateBufferSampleIndex = 0;
	usbFe->flagBufferFull = FALSE_FLAG;
    } else if ((currTransfer->status != NGENE_USB_TRANSFER_COMPLETED) && (currTransfer->status != NGENE_USB_TRANSFER_PENDING) && (currTransfer->status != NGENE_USB_TRANSFER_OFF)) {
	// if other errors, stop
	usbFe->usbStatus = currTransfer->status;
    }

    /* if samples have been copied, signal to the main thread (ReadUsbInterface) */
    if (samplesWritten == TRUE_FLAG) {
	pthread_cond_signal(&usbFe->intermediateBufferEmpty);
    }
    pthread_mutex_unlock(&usbFe->frontEndMutex);
}


int ReadUsbInterface(TRxFe *rxFe, char *buffer, int dim)
{
    TUsbFe *usbFe = &(rxFe->usbFe);
    unsigned int availableSamples = 0;
    unsigned int nSamples = 0;
    unsigned int availableSamples1 = 0;
    unsigned int availableSamples2 = 0;
    unsigned int nSamples1 = 0;
    unsigned int nSamples2 = 0;
    char *srcPtr;

    pthread_mutex_lock(&rxFe->usbFe.frontEndMutex);

    /* Verify the presence of some samples */
    if ((usbFe->usbIntermediateBufferWriteIndex == usbFe->usbIntermediateBufferReadIndex) && (usbFe->flagBufferFull == FALSE_FLAG)) {
	/* If the intermediate buffer is empty, wait */
	pthread_cond_wait(&rxFe->usbFe.intermediateBufferEmpty, &rxFe->usbFe.frontEndMutex);
    }

    /* Check again if samples are available, to avoid errors in the circular buffer */
    if ((usbFe->usbIntermediateBufferWriteIndex == usbFe->usbIntermediateBufferReadIndex) && (usbFe->flagBufferFull == FALSE_FLAG)) {
	pthread_mutex_unlock(&rxFe->usbFe.frontEndMutex);
	// still empty! zero data read
	return 0;
    }

    /* Now some samples are available */
    srcPtr = &(usbFe->usbIntermediateBuffer[(usbFe->usbIntermediateBufferReadIndex * USB_BLOCKSIZE) + usbFe->usbIntermediateBufferSampleIndex]);
    if (usbFe->usbIntermediateBufferWriteIndex > usbFe->usbIntermediateBufferReadIndex) {
	/* samples in intermediate buffer in one chunk */
	availableSamples = (usbFe->usbIntermediateBufferWriteIndex - usbFe->usbIntermediateBufferReadIndex) * USB_BLOCKSIZE;
	availableSamples -= usbFe->usbIntermediateBufferSampleIndex;
	nSamples = (availableSamples < (unsigned int) dim) ? availableSamples : (unsigned int) dim;
	ArrayCopy(srcPtr, buffer, nSamples);
    } else {
	/* samples in intermediate buffer in two chunks */
	availableSamples1 = (USB_INTERMEDIATE_BUFFER_SIZE - usbFe->usbIntermediateBufferReadIndex) * USB_BLOCKSIZE;
	availableSamples1 -= usbFe->usbIntermediateBufferSampleIndex;
	availableSamples2 = usbFe->usbIntermediateBufferWriteIndex * USB_BLOCKSIZE;
	if (availableSamples1 >= (unsigned int) dim) {
	    nSamples1 = dim;
	    nSamples2 = 0;
	} else if ((availableSamples1 + availableSamples2) >= (unsigned int) dim) {
	    nSamples1 = availableSamples1;
	    nSamples2 = dim - availableSamples1;
	} else {
	    nSamples1 = availableSamples1;
	    nSamples2 = availableSamples2;
	}
	ArrayCopy(srcPtr, buffer, nSamples1);
	ArrayCopy(usbFe->usbIntermediateBuffer, &(buffer[nSamples1]), nSamples2);
	nSamples = nSamples1 + nSamples2;
    }
    /* update read index */
    usbFe->usbIntermediateBufferSampleIndex += nSamples;
    usbFe->usbIntermediateBufferReadIndex += (usbFe->usbIntermediateBufferSampleIndex / USB_BLOCKSIZE);
    usbFe->usbIntermediateBufferReadIndex = usbFe->usbIntermediateBufferReadIndex % USB_INTERMEDIATE_BUFFER_SIZE;
    usbFe->usbIntermediateBufferSampleIndex = usbFe->usbIntermediateBufferSampleIndex % USB_BLOCKSIZE;
    if ((usbFe->flagBufferFull == TRUE_FLAG) && (usbFe->usbIntermediateBufferReadIndex != usbFe->usbIntermediateBufferWriteIndex)) {
	// now we are sure that the buffer is not full
	usbFe->flagBufferFull = FALSE_FLAG;
	// send the signal only when the buffer was full and some space is now available in the intermediate buffer
	pthread_cond_signal(&rxFe->usbFe.intermediateBufferFull);
    }
    pthread_mutex_unlock(&rxFe->usbFe.frontEndMutex);
    return nSamples;
}


int UsbCheckOverrun(TRxFe *rxFe)
{
    if (rxFe->usbFe.usbStatus == NGENE_USB_OK) {
	return SUCCESS_RETURN;
    }
    /* an error occurred: prints a message in function of the type of error */
    switch (rxFe->usbFe.usbStatus) {
    case NGENE_USB_TRANSFER_ERROR:
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "libusb error: LIBUSB_TRANSFER_ERROR");
	break;
    case NGENE_USB_TRANSFER_TIMED_OUT:
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "libusb error: LIBUSB_TRANSFER_TIMED_OUT");
	break;
    case NGENE_USB_TRANSFER_CANCELLED:
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "libusb error: LIBUSB_TRANSFER_CANCELLED");
	break;
    case NGENE_USB_TRANSFER_STALL:
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "libusb error: LIBUSB_TRANSFER_STALL");
	break;
    case NGENE_USB_TRANSFER_NO_DEVICE:
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "libusb error: LIBUSB_TRANSFER_NO_DEVICE");
	break;
    case NGENE_USB_TRANSFER_OVERFLOW:
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "libusb error: LIBUSB_TRANSFER_OVERFLOW");
	break;
    case NGENE_USB_INTERMEDIATE_BUFFER_FULL:
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "USB interface error: intermediate buffer full");
	break;
    case NGENE_USB_SUBMIT_TRANSFER_ERROR:
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "USB interface error: libusb_submit_transfer returns error");
	break;
    default:
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "USB interface error: libusb other error");
	break;
    }
    return FAILURE_RETURN;
}
