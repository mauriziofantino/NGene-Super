/**
 * @file ioFunctions.c
 *
 * @author Mario Nicola
 *
 * @date 06/08/2014
 *
 * @version 1.0.0
 *
 * @brief Functions used to perform some basic output and input operations.
 *        From a previous implementation by Andrea Molino.
 */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include "ngeneConstants.h"
#include "ioFunctions.h"
#include "ncursesApi.h"
#include <stdlib.h>


// Function to parse the input path
int ParsePath(char *line, int *vid, int *pid, char *filename, int workFromFile)
{
	int pointer;		// used to parse the string

	// default settings for the output parameters
	*vid = 0;
	*pid = 0;
	line[strlen(line)] = '\0';	// placing the end of the string
	pointer = 0;

	if(workFromFile==TRUE_FLAG)
	{
		if((sscanf(&line[pointer], "%s", filename)) != 1)
		{
			SwrxLog( NGENE_LOG_SEVERITY_ERROR,"Parse path error: %s",filename);
			return FAILURE_RETURN;
		}
	}
	else
	{
		filename = NULL;
		if((sscanf(&line[pointer], "%x/%x", vid, pid)) != 2)
		{
			return FAILURE_RETURN;
		}
	}

	return workFromFile;
}


int MsgToGuiUpdate(const char *msg, char *msgToGui)
{
	int len;
	unsigned char c = ' ';

	len = strlen(msg);
	if(len>MAX_MSG_LENGTH)
	{
		len=MAX_MSG_LENGTH;
	}
	memcpy(msgToGui,msg,len);
	if(len<(MAX_MSG_LENGTH-1))
	{
		memset(&msgToGui[len],c,MAX_MSG_LENGTH-1-len);
	}
	msgToGui[MAX_MSG_LENGTH-1]='\0';
	return 0;
}

/**
 * @def TAG_STRING_LEN
 * @brief number of char in the tagString variable in SwrxLog
 */
#define TAG_STRING_LEN   8


/* here configuration variables used for SwrxLog are defined */
static unsigned int swrxLog_verbose;
static FILE *swrxLog_stdlog;
static pthread_mutex_t *swrxLog_stdLogMutexPtr;
static TUserInterface swrxLog_userInterface;
static char *swrxLog_msgToGui;
static TNCursesData *swrxLog_ncursesData;
static int swrxLog_ncursesDataInitialized;


void InitSwrxLog(TNGeneConfiguration *cfg,TNGeneData *rx)
{
	swrxLog_verbose = cfg->verbose;
	swrxLog_stdlog = cfg->stdlog;
	swrxLog_stdLogMutexPtr = &(cfg->stdLogMutex);
	swrxLog_userInterface = cfg->userInterface;
	swrxLog_msgToGui = cfg->msgToGui;
	swrxLog_ncursesData = &(rx->ncursesData);
	swrxLog_ncursesDataInitialized = cfg->ncursesDataInitialized;
}


void RestartSwrxLog(TNGeneConfiguration *cfg)
{
	swrxLog_stdlog = cfg->stdlog;
}


int SwrxLog(int severity, const char *format, ...)
{
	FILE *f;
	time_t currentTime;
	char *timeString;
	char tagString[TAG_STRING_LEN];
	char userString[MAX_ROW_LENGTH];
	char logString[MAX_ROW_LENGTH];
	char logStringWithTime[2*MAX_ROW_LENGTH];
	va_list arguments;		/* required to handle the varying number of arguments */

	if((severity==NGENE_LOG_SEVERITY_NOTE) && (swrxLog_verbose==FALSE_FLAG))
		return SUCCESS_RETURN;	/* NOTE are not logged if verbose option is not selected */

	f = swrxLog_stdlog;
	if(f == NULL)
		return FAILURE_RETURN;		/* error in the input parameter */

	pthread_mutex_lock(swrxLog_stdLogMutexPtr);

	/* prepare the string with the current time */
	currentTime = time(NULL);
	timeString = ctime(&currentTime);
	timeString[strlen(timeString) - 1] = '\0';
	/* prepare the string with the severity information */
	switch(severity)
	{
		case NGENE_LOG_SEVERITY_ERROR:
			memcpy(tagString, "ERROR   ",TAG_STRING_LEN);
			break;
		case NGENE_LOG_SEVERITY_WARNING:
			memcpy(tagString, "WARNING ",TAG_STRING_LEN);
			break;
		case NGENE_LOG_SEVERITY_NOTE:
			memcpy(tagString, "NOTE    ",TAG_STRING_LEN);
			break;
		case NGENE_LOG_SEVERITY_INFO:
		case NGENE_LOG_SEVERITY_IMPORTANT_INFO:
			memcpy(tagString, "INFO    ",TAG_STRING_LEN);
			break;
		default:
			memcpy(tagString, "???     ",TAG_STRING_LEN);
			break;
	}
	tagString[TAG_STRING_LEN-1]='\0';
	/* prepare the string with the user information */
	va_start(arguments, format);
	vsprintf(userString, format, arguments);
	va_end(arguments);
	/* prepare the final strings */
	sprintf(logString,"%s",userString);
	sprintf(logStringWithTime,"[%s] %s: %s\n",timeString,tagString,userString);
	/* Complete the log */
	/* Behavior depends on the chosen interface:
	 * - if NCurses
	 *      - NCurses plot
	 *      - text file specified by the user (-l)
	 * - if text
	 *      - stdout
	 *      - text file specified by the user (-l)
	 * - if GUI
	 *      - string in the data packet to the GUI
	 *      - stdout
	 *      - text file specified by the user (-l)
	 */
	if((swrxLog_userInterface!=RX_UI_NCURSES) || ((f!=stdout) && (f!=stderr)))
	{
		fprintf(f,"%s",logStringWithTime);
		fflush(f);
		if((swrxLog_userInterface!=RX_UI_NCURSES) && (f!=stdout) && (f!=stderr))
		{
			fprintf(stdout,"%s",logStringWithTime);
			fflush(stdout);
		}
	}
	if(swrxLog_userInterface==RX_UI_GUI)
	{
		if((severity==NGENE_LOG_SEVERITY_ERROR)||(severity==NGENE_LOG_SEVERITY_WARNING)||(severity==NGENE_LOG_SEVERITY_IMPORTANT_INFO))
		{
			/* If error or warning, send the message to the GUI */
			MsgToGuiUpdate(logString,swrxLog_msgToGui);
		}
	}
	if((swrxLog_userInterface==RX_UI_NCURSES) && (swrxLog_ncursesDataInitialized==TRUE_FLAG))
	{
		if((severity==NGENE_LOG_SEVERITY_ERROR)||(severity==NGENE_LOG_SEVERITY_WARNING)||(severity==NGENE_LOG_SEVERITY_IMPORTANT_INFO))
		{
			/* If error or warning, send the message to the NCurses */
			NcDrawRxLog(swrxLog_ncursesData,logStringWithTime,severity);
		}
	}
	/* All logs performed */
	pthread_mutex_unlock(swrxLog_stdLogMutexPtr);
	return SUCCESS_RETURN;
}


int GetVal(char *fileName, char *fieldName, void *variable, TFieldType fieldType, int fieldLen)
{
	FILE *fin;
	int found = 0;
	char charo;
	char term;
	char termStr[2];
	char stringa[MAX_TAG_LEN];
	char tmpStr[2*MAX_ROW_LENGTH];
	int cnt;
	int firstIdx;
	int lastIdx;

	fin = fopen(fileName,"r");
	if(fin == NULL)
	{
		SwrxLog(NGENE_LOG_SEVERITY_ERROR, "ERROR: file %s not found",fileName);
		return FAILURE_RETURN;
	}
	tmpStr[0] = '\0';

	while((fscanf(fin, "%c", &charo) != EOF) && (found==0))
	{
		if(charo == '[')
		{
			fscanf(fin, "%s", stringa);
			term = ' ';
			do
			{
				fscanf(fin,"%c", &term);
				termStr[0] = term;
				termStr[1] = '\0';
				strcat(stringa, termStr);
			}
			while ((term != ']') && (term != '\n'));
			// discard up the end of the line
			while(term!='\n')
			{
				fscanf(fin,"%c", &term);
			}

			if(strstr(stringa,fieldName) != NULL)
			{
				/* Tag found */
				found = 1;
				for(cnt=0; cnt<fieldLen; cnt++)
				{
					switch(fieldType)
					{
						case F_INTEGER:
							if(!fscanf(fin,"%d", &((int *)variable)[cnt]))
							{
								fclose(fin);
								return FAILURE_RETURN;
							}
							break;

						case F_FLOAT:
							if(!fscanf(fin,"%f", &((float *)variable)[cnt]))
							{
								fclose(fin);
								return FAILURE_RETURN;
							}
							break;

						case F_DOUBLE:
							if(!fscanf(fin,"%lf", &((double *)variable)[cnt]))
							{
								fclose(fin);
								return FAILURE_RETURN;
							}
							break;
							
						case F_EXP:
							if(!fscanf(fin,"%le", &((double *)variable)[cnt]))
							{
								fclose(fin);
								return FAILURE_RETURN;
							}
							break;

						case F_STRING:
							if(cnt == 0)
								((char *)variable)[0] = '\0';

							if(!fscanf(fin,"%s", (tmpStr)))
							{
								/* String not found */
								((char *)tmpStr)[0] = '\0';
								strcat((char *)variable, tmpStr);
								fclose(fin);
								return FAILURE_RETURN;
							}
							else if((((char *)tmpStr)[0] == '[') || (((char *)tmpStr)[0] == '#')) /* String found */
							{
								/* Found an empty string, still it is a string! */
								((char *)tmpStr)[0] = '\0';
								strcat((char *)variable, tmpStr);
								fclose(fin);
								return SUCCESS_RETURN;
							}
							else
							{
								/* Concatenate the string with the previous one of the array */
								if(cnt != 0)
								{
									strcat((char *)variable, " ");
								}
								strcat((char *)variable, tmpStr);
							}
							break;

						case F_STRING_WITH_SPACES:
							if(cnt == 0)
								((char *)variable)[0] = '\0';

							//if(!fscanf(fin,"%s", (tmpStr)))
							if(fgets(tmpStr,MAX_ROW_LENGTH,fin)==NULL)
							{
								/* String not found */
								((char *)tmpStr)[0] = '\0';
								strcat((char *)variable, tmpStr);
								fclose(fin);
								return FAILURE_RETURN;
							}
							else if((((char *)tmpStr)[0] == '[') || (((char *)tmpStr)[0] == '#')) /* String found */
							{
								/* Found an empty string, still it is a string! */
								((char *)tmpStr)[0] = '\0';
								strcat((char *)variable, tmpStr);
								fclose(fin);
								return SUCCESS_RETURN;
							}
							else
							{
								// find first char different from space
								firstIdx = 0;
								while((firstIdx<(int)strlen(tmpStr)) && (isspace(tmpStr[firstIdx])))
									firstIdx++;
								// find last char different from space
								lastIdx = strlen(tmpStr)-1;
								while((lastIdx>=0) && (isspace(tmpStr[lastIdx])))
									lastIdx--;
								// save (if not empty)
								if(firstIdx<lastIdx)
								{
									tmpStr[lastIdx+1] = '\0';
									/* Concatenate the string with the previous one of the array */
									if(cnt != 0)
									{
										strcat((char *)variable, " ");
									}
									strcat((char *)variable, &(tmpStr[firstIdx]));
								}
							}
							break;

						case F_CHAR:
							if(!fscanf(fin,"%c", &((char *)variable)[cnt]))
							{
								fclose(fin);
								return FAILURE_RETURN;
							}
							break;

						default:
							if(!fscanf(fin,"%f", &((float *)variable)[cnt]))
							{
								fclose(fin);
								return FAILURE_RETURN;
							}
							break;
					}
				}
				fclose(fin);
				return SUCCESS_RETURN;
			}
		}
		else if(charo == '#')
		{
			/* when the row starts with '#', discard the whole line */
			fgets(tmpStr,2*MAX_ROW_LENGTH,fin);
		}
	}
	fclose(fin);
	return FAILURE_RETURN;
}

int GetLineCountXml(FILE *PKRFile)
{
	char *line_buf = NULL;
	size_t line_buf_size = 0;
	int line_count = 0;
	ssize_t line_size=0;

	line_size = getline(&line_buf, &line_buf_size, PKRFile);

	  /* Loop through until we are done with the file. */
	  while (line_size >= 0)
	  {
		/* Increment our line count */
		line_count++;
		line_size = getline(&line_buf, &line_buf_size, PKRFile);
	  }
	  free(line_buf);
	  return line_count;
}

int GetValXml(FILE *PKRFile, char *fieldName, void *variable, size_t *size)
{
	  char *line_buf = NULL;
	  size_t line_buf_size = 0;
	  int line_count = 0;
	  ssize_t line_size=0;
	  char *stringPointer1=NULL,*stringPointer2=NULL,*stringPointer3=NULL, *stringPointer4=NULL;
	  const char searchString[4][4]={"232","264","392","536"};
	  int index=0;
	  char term=0;
	  unsigned char exadecimalString[2*MAX_ROW_LENGTH];
	  unsigned hex;
	  int indexChar;
	  int numChar=0;
	  int lengthInBits=0;
	  char tmpStr[2*MAX_ROW_LENGTH];
	  int treeElement=0;

	  if(size!=NULL)
	  {
		  *size=0;
	  }

	  /* Get the first line of the file. */
	  line_size = getline(&line_buf, &line_buf_size, PKRFile);

	  /* Loop through until we are done with the file. */
	  while (line_size >= 0)
	  {
	    /* Increment our line count */
	    line_count++;
	    /* Show the line details */
//	    printf("line[%06d]: chars=%06zd, buf size=%06zu, contents: %s", line_count,
//	        line_size, line_buf_size, line_buf);

//	    /* Get the next line */
//	    if(lineCount==NULL)
//	    {
//	    	line_size = getline(&line_buf, &line_buf_size, PKRFile);
//	    }
	    stringPointer1=strstr(line_buf,fieldName);
	    if (stringPointer1 != NULL)
	    {
	    	if(strcmp(fieldName, TAG_LENGTH)==0)
	    	{
				for(index=0;index<4;index++)
				{
					 stringPointer2=strstr(line_buf,searchString[index]);
					 if(stringPointer2!= NULL)
					 {
						 if(sscanf(stringPointer2,"%d", &((int *)variable)[0]))
						 {
							 free(line_buf);
							 return SUCCESS_RETURN;
						 }
					 }
				}
				free(line_buf);
				return FAILURE_RETURN;
	    	}
	    	else if(strcmp(fieldName, TAG_PKID)==0)
	    	{
	    		stringPointer2=strstr(line_buf,TAG_PKID);
	    		if(stringPointer2!= NULL)
	    		{
	    			stringPointer2 += strlen(fieldName);
	    			if(sscanf(stringPointer2,"%d", &((int *)variable)[0]))
	    			{
	    				free(line_buf);
	    				return SUCCESS_RETURN;
	    			}
				}
				free(line_buf);
				return FAILURE_RETURN;
	    	}
		    else if(strcmp(fieldName, TAG_EXADECIMAL_STRING_START)==0)
		    {
		        stringPointer2=strstr(line_buf,TAG_EXADECIMAL_STRING_END);
				if(stringPointer2!= NULL)
				{
					numChar=0;
					while(term!='<')
					{
						sscanf(stringPointer1+strlen(fieldName),"%c", &term);
						stringPointer1++;
						numChar++;
					}
					memcpy(exadecimalString, stringPointer1+strlen(fieldName)-numChar,(numChar-1));
					((char *)tmpStr)[0] = '\0';
					memcpy(&exadecimalString[numChar-1],&tmpStr[0],1);
					//**
					indexChar=0;
					while(1==sscanf(&((const char *)exadecimalString)[indexChar], "%2x", &hex))
					{
						((char *)variable)[(*size)]=(unsigned char)hex;
						(*size)++;
						indexChar=indexChar+2;
					}
					//**
					free(line_buf);
					return SUCCESS_RETURN;
				}
				else
				{
					free(line_buf);
					return FAILURE_RETURN;
				}
		    }
		    else if(strcmp(fieldName, TAG_MT_NODE)==0)
		    {
		    	strcpy(fieldName, TAG_MT_LEVEL);
		    }
		    else if(strcmp(fieldName, TAG_MT_LEVEL)==0)
		    {
		    	sscanf(stringPointer1+strlen(fieldName),"%d", &treeElement);
		    	if(treeElement==4)
		    	{
		    		strcpy(fieldName, TAG_MT_INDEX);
		    	}
		    	else
		    	{
		    		strcpy(fieldName, TAG_MT_NODE);
		    	}
		    }
		    else if(strcmp(fieldName, TAG_MT_INDEX)==0)
		    {
		    	sscanf(stringPointer1+strlen(fieldName),"%d", &treeElement);
		    	if(treeElement==0)
		    	{
		    		line_size = getline(&line_buf, &line_buf_size, PKRFile);
	    		    stringPointer2=strstr(line_buf,TAG_LENGTH);
				    /* Show the line details */
//				    printf("line[%06d]: chars=%06zd, buf size=%06zu, contents: %s", line_count,
//				        line_size, line_buf_size, line_buf);
				    if(stringPointer2!=NULL)
				    {
		    			sscanf(stringPointer2+strlen(TAG_LENGTH),"%d", &(lengthInBits));
		    		    line_size = getline(&line_buf, &line_buf_size, PKRFile);
					    /* Show the line details */
//					    printf("line[%06d]: chars=%06zd, buf size=%06zu, contents: %s", line_count,
//					        line_size, line_buf_size, line_buf);
		    		    stringPointer3=strstr(line_buf,TAG_MT_ELEMENT_START);
		    		    stringPointer4=strstr(line_buf,TAG_MT_ELEMENT_END);
		    		    if(stringPointer3!= NULL || stringPointer4!= NULL)
		    		    {
							numChar=0;
							while(term!='<')
							{
								sscanf(stringPointer3+strlen(TAG_MT_ELEMENT_START),"%c", &term);
								stringPointer3++;
								numChar++;
							}
							memcpy(exadecimalString, stringPointer3+strlen(TAG_MT_ELEMENT_START)-numChar,(numChar-1));
							((char *)tmpStr)[0] = '\0';
							memcpy(&exadecimalString[numChar-1],&tmpStr[0],1);
							//**
							indexChar=0;
							while(1==sscanf(&((const char *)exadecimalString)[indexChar], "%2x", &hex))
							{
								((char *)variable)[(*size)]=(unsigned char)hex;
								(*size)++;
								indexChar=indexChar+2;
							}
							free(line_buf);
							return SUCCESS_RETURN;
		    		    }
					}
				}
				else
				{
					strcpy(fieldName, TAG_MT_NODE);
				}
			}
		    else if(strcmp(fieldName, MT_ROOT_NODE_START)==0)
			{
		    	stringPointer2=strstr(line_buf,MT_ROOT_NODE_START);
		    	if(stringPointer2!= NULL)
				{
					numChar=0;
					while(term!='<')
					{
						sscanf(stringPointer2+strlen(MT_ROOT_NODE_START),"%c", &term);
						stringPointer2++;
						numChar++;
					}
					memcpy(exadecimalString, stringPointer2+strlen(MT_ROOT_NODE_START)-numChar,(numChar-1));
					((char *)tmpStr)[0] = '\0';
					memcpy(&exadecimalString[numChar-1],&tmpStr[0],1);
					//**
					indexChar=0;
					while(1==sscanf(&((const char *)exadecimalString)[indexChar], "%2x", &hex))
					{
						((char *)variable)[(*size)]=(unsigned char)hex;
						(*size)++;
						indexChar=indexChar+2;
					}
					free(line_buf);
					return SUCCESS_RETURN;
				}
			}
		}
	    line_size = getline(&line_buf, &line_buf_size, PKRFile);
	}
	free(line_buf);
	return FAILURE_RETURN;
}



int ReadAnInt(void **ptr)
{
    int tmp;
    tmp = *((int *) *ptr);
    *ptr = (void *) (((int *) *ptr) + 1);
    return tmp;
}

float ReadAFloat(void **ptr)
{
    float tmp;
    tmp = *((float *) *ptr);
    *ptr = (void *) (((float *) *ptr) + 1);
    return tmp;
}

double ReadADouble(void **ptr)
{
    double tmp;
    tmp = *((double *) *ptr);
    *ptr = (void *) (((double *) *ptr) + 1);
    return tmp;
}

void WriteAShort(void **ptr,short int value)
{
	*((short int *)(*ptr)) = value;
	 *ptr = (void *) (((short int *) *ptr) + 1);
}


void WriteAnInt(void **ptr,int value)
{
	*((int *)(*ptr)) = value;
	 *ptr = (void *) (((int *) *ptr) + 1);
}

void WriteAChar(void **ptr,char value)
{
	*((char *)(*ptr)) = value;
	 *ptr = (void *) (((char *) *ptr) + 1);
}

void WriteAnUnsignedChar(void **ptr,unsigned char value)
{
	*((unsigned char *)(*ptr)) = value;
	 *ptr = (void *) (((unsigned char *) *ptr) + 1);
}

void WriteAnUnsignedInt(void **ptr,unsigned int value)
{
	*((unsigned int *)(*ptr)) = value;
	 *ptr = (void *) (((unsigned int *) *ptr) + 1);
}

void WriteAnUnsignedLongLongInt(void **ptr,unsigned long long int value)
{
	*((unsigned long long int *)(*ptr)) = value;
	 *ptr = (void *) (((unsigned long long int *) *ptr) + 1);
}


void WriteAFloat(void **ptr,float value)
{
	*((float *)(*ptr)) = value;
	*ptr = (void *) (((float *) *ptr) + 1);
}

void WriteADouble(void **ptr,double value)
{
	*((double *)(*ptr)) = value;
	*ptr = (void *) (((double *) *ptr) + 1);
}

void WriteALongLongInt(void **ptr,long long int value)
{
	*((long long int*)(*ptr)) = value;
	*ptr = (void *) (((long long int *) *ptr) + 1);
}

void WriteAString(void **ptr,char *array,int arrayLength)
{
	strncpy((char *)(*ptr),array,arrayLength);
	*ptr = (void *) (((char *) *ptr) + arrayLength);
}

void PrintABitString(FILE *outFile,unsigned char *array,int numBits)
{
	unsigned char mask = 0x80;
	int byteIdx = 0;
	int bitIdx;

	if(outFile==NULL)
	{
		return;
	}

	for(bitIdx=0;bitIdx<numBits;bitIdx++)
	{
		if((array[byteIdx]&mask)>0)
		{
			fprintf(outFile,"1");
		}
		else
		{
			fprintf(outFile,"0");
		}
		mask >>= 1;
		if(mask==0)
		{
			byteIdx++;
			mask = 0x80;
		}
	}
}

int CheckFileExistence(char *filename)
{
	FILE *tmp = fopen(filename,"r");

	if(tmp==NULL)
		return FALSE_FLAG;

	fclose(tmp);
	return TRUE_FLAG;
}
