/**
 * @file checkFolder.c
 *
 * @author Mario Nicola
 *
 * @date 24/08/2020
 *
 * @version 1.0.0
 *
 * @brief Function used to che the existence of a folder
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include "ngeneConstants.h"

int IsFolderExistent(char *folder)
{
    struct stat sb;

    if(stat(folder, &sb) == 0 && S_ISDIR(sb.st_mode))
    {
        // it exists
    	return TRUE_FLAG;
    }
    else
    {
    	// it does not exist
    	return FALSE_FLAG;
    }
}


int GetFilenameFromFilePointer(FILE *fp,char *filename)
{
	char tmpPath[MAX_ROW_LENGTH]; // temp variable used to get the filename
	char outPath[MAX_ROW_LENGTH]; // complete path of the target file
	int len;
	int idx;

	// terminate the output string
	filename[0] = '\0';

	// if the file pointer is not valid, terminate!
	if(fp==NULL)
	{
		return FALSE_FLAG;
	}

	// read file descriptor and use to address the file in proc
	sprintf(tmpPath,"/proc/self/fd/%d",fileno(fp));
	// get the complete path of the target file
	len = readlink(tmpPath, outPath, MAX_ROW_LENGTH-1);
	if((len<1) || (len>=MAX_ROW_LENGTH))
	{
		// something went wrong
		return FALSE_FLAG;
	}
	// the path string is not terminated
	outPath[len] = '\0';
	// extract the filename (find the last '/')
	idx = len-1;
	while((idx>=0) && (outPath[idx]!='/'))
		idx--;
	if(idx<0)
		idx = 0;
	// output
	sprintf(filename,"%s",&(outPath[idx]));
	return TRUE_FLAG;
}
