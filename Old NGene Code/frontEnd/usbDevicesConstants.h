/**
 * @file usbDevicesConstants.h
 *
 * @author Mario Nicola
 *
 * @date 24/09/2014
 *
 * @version 1.0.1
 *
 * @brief Constants related to the different supported front-ends
 */

#ifndef USB_DEVICES_CONSTANTS_H_INCLUDED
#define USB_DEVICES_CONSTANTS_H_INCLUDED

/**
 * @def USB_WAIT_NS
 * @brief When CopySamplesIntoUsbIntermediateBuffer terminates, no completed USB transfers are available,
 *        the USB thread sleep this time to allow the main to be scheduled
 */
#define USB_WAIT_NS  1000


/* default values */
/* TODO Verificare SiGE e Fraunhofer!!!
 *      Completare con i valori mancanti (vedi STEREO) */
#define DEFAULT_CONFIG			(1)
#define DEFAULT_RX_ENDPOINTS 	(6)
#define DEFAULT_VRT_VENDOR_IN 	(0xc0)
#define DEFAULT_VRT_VENDOR_OUT 	(0x40)
#define DEFAULT_RX_INTERFACE 	(2)
#define DEFAULT_RX_ALTINTERFACE (0)
#define DEFAULT_VRQ_START_XFER	(0x01)
#define DEFAULT_VRQ_STOP_XFER   (0x00)
#define DEFAULT_WVALUE 			(0x01)

/* SiGE */
#define SIGE_CONFIG       		(1)
#define SIGE_INTF         		(2)
#define SIGE_ALTINTF      		(0)
#define SIGE_RX_EP  			(0x86)
#define SIGE_VRQT_VENDOR_OUT	(0x40)
#define SIGE_VRQ_START_XFER		(0x01)
#define SIGE_VRQ_STOP_XFER   	(0x00)
#define SIGE_WVALUE 			(0x01)

/* NVS Stereo */
#define STEREO_CONFIG       	(1)
#define STEREO_INTF         	(0)
#define STEREO_ALTINTF      	(0)
#define STEREO_RX_EP  			(0x86)
#define STEREO_VRQT_VENDOR_OUT	(0x40)
#define STEREO_VRQ_START_XFER	(0x01)
#define STEREO_VRQ_STOP_XFER   	(0x00)
#define STEREO_WVALUE 			(0x00)

/* Amungo NT1065 */
#define NT1065_CONFIG       	(1)
#define NT1065_INTF         	(0)
#define NT1065_ALTINTF      	(0)
#define NT1065_RX_EP  			(0x81)

/* USRP B200 (???) */
#define USRP_B200_CONFIG       	(1)
#define USRP_B200_INTF         	(2)
#define USRP_B200_ALTINTF      	(0)
#define USRP_B200_RX_EP			(0x86)





#endif				/* USB_DEVICES_CONSTANTS_H_INCLUDED */
