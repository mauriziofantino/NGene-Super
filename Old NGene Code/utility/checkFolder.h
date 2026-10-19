/**
 * @file checkFolder.h
 *
 * @author Mario Nicola
 *
 * @date 24/08/2020
 *
 * @version 1.0.0
 *
 * @brief Function used to che the existence of a folder
 */
#ifndef UTILITY_CHECKFOLDER_H_INCLUDED
#define UTILITY_CHECKFOLDER_H_INCLUDED

/**
 * @brief Check if the specified folder exists.
 *
 * @param folder points to the folder
 *
 * @return TRUE_FLAG when the folder exists, FALSE_FLAG elsewhere
 */
int IsFolderExistent(char *folder);


/**
 * @brief Get the file name associated to a file pointer.
 *        Works on Linux only.
 *
 * @param fp is the file pointer initialized with fopen
 * @param filename point to the output file name
 *
 * @return TRUE_FLAG when filename is valid, FALSE_FLAG elsewhere
 */
int GetFilenameFromFilePointer(FILE *fp,char *filename);


#endif /* UTILITY_CHECKFOLDER_H_INCLUDED */
