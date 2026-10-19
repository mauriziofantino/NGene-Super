/**
 * @file frontend.c
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

#include <stdio.h>
#include <stdlib.h>
#include "frontend.h"
#include "usbInterface.h"
#include "ngeneConfiguration.h"
#include "ngeneConstants.h"
#include "ioFunctions.h"
#include "closeManager.h"


int InitializeRxFrontEnd(TNGeneConfiguration *cfg, TRxFe *rxFe)
{
    TUsbFe *usbFe = &(rxFe->usbFe);

    /* default values */
    rxFe->feInitialized = FALSE_FLAG;
    rxFe->incomingFile = NULL;
    usbFe->usbBuffer = NULL;
    usbFe->usbBufferIndex = 0;
    usbFe->usbIntermediateBuffer = NULL;
    usbFe->usbIntermediateBufferWriteIndex = 0;
    usbFe->usbIntermediateBufferReadIndex = 0;
    usbFe->usbIntermediateBufferSampleIndex = 0;
    usbFe->vid = 0;
    usbFe->pid = 0;
    usbFe->configuration = 0;
    usbFe->interface = 0;
    usbFe->altInterface = 0;
    usbFe->endpoint = 0;
    usbFe->vrqtVendorOut = 0;
    usbFe->vrqStartXfer = 0;
    usbFe->vrqStopXfer = 0;
    usbFe->wValue = 0;
    usbFe->epHandle = NULL;
    usbFe->usbStatus = NGENE_USB_OK;

    /* initialization dependent on the type of input */
    rxFe->inputMethod = cfg->inputMethod;
    if (cfg->inputMethod == INPUT_FILE) {
	rxFe->incomingFile = fopen(cfg->sampleFilename, "rb");
	if (rxFe->incomingFile == NULL) {
	    SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Front End: unable to open Front End (file)");
	    CloseNGene(cfg, EXIT_FAILURE);
	}
    } else if (cfg->inputMethod == USB_FRONTEND) {
	if (OpenUsbFrontend(cfg, rxFe) == FAILURE_RETURN) {
	    SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Front End: unable to open Front End (usb)");
	    CloseNGene(cfg, EXIT_FAILURE);
	}
    }
    rxFe->feInitialized = TRUE_FLAG;
    return SUCCESS_RETURN;
}

// Wrapper function to shut down the Front End together with its data structure
int DeinitializeRxFrontEnd(TNGeneConfiguration *cfg, TRxFe *rxFe)
{
    if (rxFe->feInitialized == FALSE_FLAG) {
	return SUCCESS_RETURN;
    }

    if ((cfg->inputMethod == INPUT_FILE) && (rxFe->incomingFile != NULL)) {
	/* input from file */
	fclose(rxFe->incomingFile);
	rxFe->incomingFile = NULL;
    } else if (cfg->inputMethod == USB_FRONTEND) {
	/* input from USB */
	CloseUsbFrontend(rxFe);
    }
    rxFe->feInitialized = FALSE_FLAG;
    return SUCCESS_RETURN;
}

// Wrapper function to enable the data transmission from the Front End
int StartRxFrontEnd(TNGeneConfiguration *cfg, TRxFe *rxFe)
{
    if (rxFe->feInitialized == FALSE_FLAG) {
	return SUCCESS_RETURN;
    }
    /* Actually StartRxFrontEnd is necessary just with the USB  */
    if (cfg->inputMethod == INPUT_FILE) {
	return SUCCESS_RETURN;
    }
    /* Starts the USB data transfers */
    if (StartUsbInterface(rxFe) == FAILURE_RETURN) {
	SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Front End: cannot start the Front End");
	return FAILURE_RETURN;
    }
    return SUCCESS_RETURN;
}

// Shut down the Front End
int StopRxFrontEnd(TRxFe *rxFe)
{
    if (rxFe->feInitialized == FALSE_FLAG) {
	return SUCCESS_RETURN;
    }
    if (rxFe->incomingFile != NULL) {
	fclose(rxFe->incomingFile);
	rxFe->incomingFile = NULL;
    } else if (rxFe->inputMethod == USB_FRONTEND) {
	if (StopUsbInterface(rxFe) == FAILURE_RETURN) {
	    SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Front End: cannot stop the Front End");
	    return FAILURE_RETURN;
	}
    }
    return SUCCESS_RETURN;
}

// Verify if the Front End is working properly
int FrontEndCheckStatus(TRxFe *rxFe)
{
    if (rxFe->incomingFile != NULL) {
	if (feof(rxFe->incomingFile)) {
	    SwrxLog(NGENE_LOG_SEVERITY_ERROR, "Front End: reached EOF");
	    return FAILURE_RETURN;
	}
    } else if (rxFe->inputMethod == USB_FRONTEND) {
	if (UsbCheckOverrun(rxFe) == FAILURE_RETURN) {
	    SwrxLog(NGENE_LOG_SEVERITY_NOTE, "Front End: USB errors found");
	    return FAILURE_RETURN;
	}
    }
    return SUCCESS_RETURN;
}

// Get the data from the correct input
unsigned int ReadRxFrontEnd(TRxFe *rxFe, char *buffer, int dim)
{
    unsigned int readByte = 0;

    if (rxFe->incomingFile != NULL) {
	readByte = (unsigned int) fread(buffer, sizeof(char), dim, rxFe->incomingFile);
    } else if (rxFe->inputMethod == USB_FRONTEND) {
	readByte = (unsigned int) ReadUsbInterface(rxFe, buffer, dim);
    }
    return readByte;
}
