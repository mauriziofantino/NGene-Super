/**
 * @file frontendThreadFunctions.h
 *
 * @author Micaela Troglia Gamba
 *
 * @date 09/12/2014
 *
 * @version 1.0.1
 *
 * @brief Function performed in a separate thread that manages USB Front End
 */

#ifndef FRONTEND_THREAD_FUNCTIONS_H_INCLUDED
#define FRONTEND_THREAD_FUNCTIONS_H_INCLUDED

#include "ngeneDataStructure.h"


typedef struct FrontEndThreadFunctionParametersStruct {
    TNGeneConfiguration *ngeneConfiguration;
    TRxFe *ngeneFrontEnd;
} TFrontEndThreadFunctionParameters;


/**
 * @brief Initializes mutex and condition variables of the UsbFrontEndThreadFunction
 *
 * @param ngeneFe points to fe structure
 * @param frontEndThreadFunctionParameters points to a structure containing all variable related to the FE
 *
 * @return void
 */
#ifndef MONITOR_INTERMEDIATE_BUFFER
void InitUsbFrontEndThreadFunction(TRxFe * ngeneFe, TFrontEndThreadFunctionParameters * frontEndThreadFunctionParameters);
#else
void InitUsbFrontEndThreadFunction(TRxFe * ngeneFe, TFrontEndThreadFunctionParameters * frontEndThreadFunctionParameters, TNGeneConfiguration * ngeneCfg);
#endif

/**
 * @brief Destroy mutex and condition variables of the UsbFrontEndThreadFunction
 *
 * @param ngeneFe points to fe structure
 * @param frontEndThreadFunctionParameters points to a structure containing all variable related to the FE
 *
 * @return void
 */
void DeinitUsbFrontEndThreadFunction(TRxFe * ngeneFe, TFrontEndThreadFunctionParameters * frontEndThreadFunctionParameters);

/**
 * @brief It handles all functions related to the FE USB, and it is in charge of reading the USB stream and writing it onto the intermediate buffer.
 *
 * @param frontEndThreadFunctionParameters points to a structure containing all variable related to the FE
 *
 * @return void
 */
void *UsbFrontEndThreadFunction(TFrontEndThreadFunctionParameters * frontEndThreadFunctionParameters);


/**
 * @brief Initializes mutex and condition variables of the MqFrontEndThreadFunction
 *
 * @param ngeneFe points to fe structure
 * @param frontEndThreadFunctionParameters points to a structure containing all variable related to the FE
 *
 * @return void
 */
#ifndef MONITOR_INTERMEDIATE_BUFFER
void InitMqFrontEndThreadFunction(TRxFe * ngeneFe, TFrontEndThreadFunctionParameters * frontEndThreadFunctionParameters);
#else
void InitMqFrontEndThreadFunction(TRxFe * ngeneFe, TFrontEndThreadFunctionParameters * frontEndThreadFunctionParameters, TNGeneConfiguration * ngeneCfg);
#endif
/**
 * @brief Destroy mutex and condition variables of the MqFrontEndThreadFunction
 *
 * @param ngeneFe points to fe structure
 * @param frontEndThreadFunctionParameters points to a structure containing all variable related to the FE
 *
 * @return void
 */
void DeinitMqFrontEndThreadFunction(TRxFe * ngeneFe, TFrontEndThreadFunctionParameters * frontEndThreadFunctionParameters);

/**
 * @brief Set the real-time Scheduling Priority and Policy of the CPU
 *
 * @param indexCpu indicates the index of the CPU
 * @param policy is the real-time policy: SCHED_FIFO, SCHED_RR, or SCHED_OTHERS
 * @param indexThread indicates the index of the thread, created from the main thread, in charge of the channel elaboration
 * @return void
 */
void SetCpuPriorityAndPolicy(int indexCpu, int policy, int priority);

#endif				/* FRONTEND_THREAD_FUNCTIONS_H_INCLUDED */
