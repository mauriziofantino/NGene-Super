/**
 * @file ioFunctions.h
 *
 * @author Mario Nicola
 *
 * @date 06/08/2014
 *
 * @version 1.0.0
 *
 * @brief Functions used to perform some basic output and input operations.
 */

#ifndef IO_FUNCTIONS_H_INCLUDED
#define IO_FUNCTIONS_H_INCLUDED

#include <stdio.h>
#include "ngeneConfiguration.h"
#include "ngeneDataStructure.h"

/*
 * @def NGENE_LOG_SEVERITY_NOTE
 * @brief Defines a low severity message. Printed only when the user specify the verbose flag.
 */
#define NGENE_LOG_SEVERITY_NOTE 	0
/*
 * @def NGENE_LOG_SEVERITY_WARNING
 * @brief Defines a warning message.
 */
#define NGENE_LOG_SEVERITY_WARNING 	1
/*
 * @def NGENE_LOG_SEVERITY_ERROR
 * @brief Defines an error message.
 */
#define NGENE_LOG_SEVERITY_ERROR 	2
/*
 * @def NGENE_LOG_SEVERITY_INFO
 * @brief Defines an information message.
 *        Used like NGENE_LOG_SEVERITY_NOTE, but it is printed independently from the verbose flag.
 *        Not printed in the ncurses interface.
 */
#define NGENE_LOG_SEVERITY_INFO 	3

/*
 * @def NGENE_LOG_SEVERITY_IMPORTANT_INFO
 * @brief Defines an information message.
 *        Used like NGENE_LOG_SEVERITY_NOTE, but it is printed independently from the verbose flag.
 *        It is printed also in the ncurses interface.
 */
#define NGENE_LOG_SEVERITY_IMPORTANT_INFO 	4


/**
 * @enum FieldTypeEnum
 *
 * @brief Describes the type of data to be read from a configuration file
 */
typedef enum FieldTypeEnum
{
	F_INTEGER,
	F_FLOAT,
	F_DOUBLE,
	F_EXP,
	F_STRING,
	F_STRING_WITH_SPACES,
	F_CHAR
} TFieldType;


/**
 * @brief ParsePath parses the string used to specify the source of the input samples
 *
 * @param line is the string that will be parsed
 * @param vid points to the variable that will store VID (input from USB)
 * @param pid points to the variable that will store PID (input from USB)
 * @param filename is the string  indicating the path of the input file (input from binary file)
 *
 * @return SUCCESS_RETURN when the desired value is read, FAILURE_RETURN otherwise
 */
int ParsePath(char *line, int *vid, int *pid, char *filename, int workFromFile);


/**
 * @brief InitSwrxLog initializes some variables used by SwrxLog
 *
 * @param cfg points to the receiver configuration (log file included)
 * @params rx points to the receiver data structure
 */
void InitSwrxLog(TNGeneConfiguration *cfg,TNGeneData *rx);

/**
 * @brief RestartSwrxLog updates the pointer to the log (for the split)
 *
 * @param cfg points to the receiver configuration (log file included)
 */
void RestartSwrxLog(TNGeneConfiguration *cfg);


/**
 * @brief SwrxLog prints a message to the user. Behavior depends on the chosen interface
 *
 * @param severity is an enumerate that specifies the severity of the message
 * @param format is the string that specifies what will be printed (like stdio.h fprintf)
 *
 * @return SUCCESS_RETURN when the desired value is read, FAILURE_RETURN otherwise
 */
int SwrxLog(int severity, const char *format, ...);


/**
 * @brief GetVal reads a specified data from a configuration file.
 *
 * @details The read parameters is indicated through a tag that must
 *          be found in the configuration file.
 *
 * @param fileName is the name of the configuration file
 * @param fieldName is the tag indicating the parameter to be read
 * @param variable points to the variable that will store the read parameter
 * @param fieldType indicates the type of parameter that will be read
 * @param fieldLen how many value wil be read
 *
 * @return SUCCESS_RETURN when the desired value is read, FAILURE_RETURN otherwise
 */
int GetVal(char *fileName, char *fieldName, void *variable, TFieldType fieldType, int fieldLen);

int GetLineCountXml(FILE *PKRFile);

int GetValXml(FILE *PKRFile, char *fieldName, void *variable, size_t *size);
/**
 * @brief readAnInt reads an integer value from a void pointer,
 * 	      then updates the pointer
 *
 * @param ptr points to the pointer to the memory location storing the int value
 *
 * @return the read int value
 */
int ReadAnInt(void **ptr);

/**
 * @brief readAFloat reads a float value from a void pointer,
 * 	      then updates the pointer
 *
 * @param ptr points to the pointer to the memory location storing the float value
 *
 * @return the read float value
 */
float ReadAFloat(void **ptr);

/**
 * @brief readADouble reads a double value from a void pointer,
 * 	      then updates the pointer
 *
 * @param ptr points to the pointer to the memory location storing the double value
 *
 * @return the read double value
 */
double ReadADouble(void **ptr);

/**
 * @brief WriteAnInt writes an int value in the location pointed by a void pointer,
 * 	      then updates the pointer value
 *
 * @param ptr points to the pointer to the memory location that will store the int value
 * @param value is the value that will be written
 */
void WriteAnInt(void **ptr,int value);

/**
 * @brief WriteAShort writes a short int value in the location pointed by a void pointer,
 * 	      then updates the pointer value
 *
 * @param ptr points to the pointer to the memory location that will store the short int value
 * @param value is the value that will be written
 */
void WriteAShort(void **ptr,short int value);

/**
 * @brief WriteAnUnsignedInt writes an unsigned int value in the location pointed by a void pointer,
 * 	      then updates the pointer value
 *
 * @param ptr points to the pointer to the memory location that will store the unsigned int value
 * @param value is the value that will be written
 */
void WriteAnUnsignedInt(void **ptr,unsigned int value);

/**
 * @brief WriteAnUnsignedLongLongInt writes an unsigned long lomng int value in the location pointed by a void pointer,
 * 	      then updates the pointer value
 *
 * @param ptr points to the pointer to the memory location that will store the unsigned long long int value
 * @param value is the value that will be written
 */
void WriteAnUnsignedLongLongInt(void **ptr,unsigned long long int value);

/**
 * @brief WriteAChar writes an char value in the location pointed by a void pointer,
 * 	      then updates the pointer value
 *
 * @param ptr points to the pointer to the memory location that will store the char value
 * @param value is the value that will be written
 */
void WriteAChar(void **ptr,char value);

/**
 * @brief WriteAnUnsignedChar writes an unsigned char value in the location pointed by a void pointer,
 * 	      then updates the pointer value
 *
 * @param ptr points to the pointer to the memory location that will store the unsigned char value
 * @param value is the value that will be written
 */
void WriteAnUnsignedChar(void **ptr,unsigned char value);


/**
 * @brief WriteAFloat writes a float value in the location pointed by a void pointer,
 * 	      then updates the pointer value
 *
 * @param ptr points to the pointer to the memory location that will store the float value
 * @param value is the value that will be written
 */
void WriteAFloat(void **ptr,float value);

/**
 * @brief WriteADouble writes a double value in the location pointed by a void pointer,
 * 	      then updates the pointer value
 *
 * @param ptr points to the pointer to the memory location that will store the double value
 * @param value is the value that will be written
 */
void WriteADouble(void **ptr,double value);

/**
 * @brief WriteALongLongInt writes a long long int value in the location pointed by a void pointer,
 * 	      then updates the pointer value
 *
 * @param ptr points to the pointer to the memory location that will store the long long int value
 * @param value is the value that will be written
 */
void WriteALongLongInt(void **ptr,long long int value);

/**
 * @brief WriteAString copy a string in the location pointed by a void pointer,
 * 	      then updates the pointer value
 *
 * @param ptr points to the pointer to the memory location that will store the string
 * @param array is the string to be copied
 * @param arrayLength is the number of char that will be copied
 */
void WriteAString(void **ptr,char *array,int arrayLength);

/**
 * @brief PrintABitString print a string of 1 and 0 starting from a sequence of bytes.
 *        This string is printed in the text file pointed by outFile. At the end of the
 *        string of bits, no newline ('\n') is added.
 *
 * @param outFile points to the output file (if NULL nothing is printed)
 * @param array points to the bits to be printed (as a sequence of bytes)
 * @param numBits is the number of bits to be printed. If it is not a multiple
 *        of 8, remaining bits are ignored.
 */
void PrintABitString(FILE *outFile,unsigned char *array,int numBits);


/**
 * @brief Check if the file with the given filename does exist or not
 *
 * @param filename is the complete path to the desired file
 *
 * @return TRUE_FLAG if the file exists, FALSE_FLAG elsewhere
 */
int CheckFileExistence(char *filename);

#endif /* IO_FUNCTIONS_H_INCLUDED */
