/**
 * @file argvParser.h
 *
 * @author Mario Nicola
 *
 * @date 06/08/2014
 *
 * @version 1.0.0
 *
 * @brief Functions used to parse the command line arguments.
 */

#ifndef ARGV_PARSER_H_INCLUDED
#define ARGV_PARSER_H_INCLUDED

#include "ngeneConfiguration.h"

/**
 * @brief GetMainName parses command line arguments to extract program name.
 *        It excludes the path.
 *
 * @param argv stores command line parameters
 * @param cfg points to the receiver configuration
 */
void GetMainName(char *argv[], TNGeneConfiguration *cfg);

/**
 * @brief HandleCmdLine parses command line arguments to extract program options.
 *
 * @param argc is the number of command line arguments
 * @param argv stores command line parameters
 * @param cfg points to the receiver configuration
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int HandleCmdLine(int argc, char *argv[], TNGeneConfiguration *cfg);

#endif /* ARGV_PARSER_H_INCLUDED */
