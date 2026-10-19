/**
 * @file fnavMessageParser.h
 *
 * @author Mario Nicola
 *
 * @date 08/02/2021
 *
 * @version 1.0.0
 *
 * @brief Functions used to parse the Galileo F/NAV navigation message.
 */

#ifndef FNAV_MESSAGE_PARSER_H_INCLUDED
#define FNAV_MESSAGE_PARSER_H_INCLUDED



/**
 * @brief Initialize the data structure used for parse the Galileo F/NAV navigation message
 *
 * @param cfg points to the receiver configuration
 * @param ch points to the current channel
 *
 * @return SUCCESS_RETURN when the function ends with success, FAILURE_RETURN otherwise
 */
int InitFNavParser(TNGeneConfiguration *cfg,TChannel *ch);

/**
 * @brief Deinit the data structure used for parse the Galileo F/NAV navigation message
 *
 * @param ch points to the current channel
 */
void DeinitFNavParser(TChannel *ch);

/**
 * @brief Called at the start of the tracking, it prepares the parse of the navigation message
 *
 * @param cfg points to the receiver configuration
 * @param rx points to the receiver base data structure
 * @param ch points to the current channel
 */
void StartFNavParser(TNGeneConfiguration *cfg,TNGeneData *rx,TChannel *ch);

/**
 * @brief Manages the parse of the Galileo F/NAV navigation message.
 *
 * @param rx points to the receiver base data structure
 * @param ch points to the current channel
 * @param corr is the correlation related to the last received bit
 */
void ParseFNavMessage(TNGeneData *rx,TChannel *ch,float corr);


/* description of the navigation message format */
#define FNAV_WORDTYPE_FIRST_BYTE_IDX 			(0)
#define FNAV_WORDTYPE_LAST_BYTE_IDX  			(0)
#define FNAV_WORDTYPE_BITS_OF_FIRST_BYTE 		(6)
#define FNAV_WORDTYPE_BITS_OF_LAST_BYTE 		(0)

#define FNAV_TYPE1_SVID_FIRST_BYTE_IDX 			(0)
#define FNAV_TYPE1_SVID_LAST_BYTE_IDX 			(1)
#define FNAV_TYPE1_SVID_BITS_OF_FIRST_BYTE 		(2)
#define FNAV_TYPE1_SVID_BITS_OF_LAST_BYTE 		(4)

#define FNAV_TYPE1_IOD_FIRST_BYTE_IDX 			(1)
#define FNAV_TYPE1_IOD_LAST_BYTE_IDX 			(2)
#define FNAV_TYPE1_IOD_BITS_OF_FIRST_BYTE 		(4)
#define FNAV_TYPE1_IOD_BITS_OF_LAST_BYTE 		(6)

#define FNAV_TYPE1_WN_FIRST_BYTE_IDX 			(19)
#define FNAV_TYPE1_WN_LAST_BYTE_IDX 			(20)
#define FNAV_TYPE1_WN_BITS_OF_FIRST_BYTE 		(5)
#define FNAV_TYPE1_WN_BITS_OF_LAST_BYTE 		(7)

#define FNAV_TYPE1_TOW_FIRST_BYTE_IDX 			(20)
#define FNAV_TYPE1_TOW_LAST_BYTE_IDX 			(23)
#define FNAV_TYPE1_TOW_BITS_OF_FIRST_BYTE 		(1)
#define FNAV_TYPE1_TOW_BITS_OF_LAST_BYTE 		(3)

#define FNAV_TYPE2_IOD_FIRST_BYTE_IDX 			(0)
#define FNAV_TYPE2_IOD_LAST_BYTE_IDX 			(1)
#define FNAV_TYPE2_IOD_BITS_OF_FIRST_BYTE 		(2)
#define FNAV_TYPE2_IOD_BITS_OF_LAST_BYTE 		(8)

#define FNAV_TYPE2_WN_FIRST_BYTE_IDX 			(22)
#define FNAV_TYPE2_WN_LAST_BYTE_IDX 			(24)
#define FNAV_TYPE2_WN_BITS_OF_FIRST_BYTE 		(2)
#define FNAV_TYPE2_WN_BITS_OF_LAST_BYTE 		(2)

#define FNAV_TYPE2_TOW_FIRST_BYTE_IDX 			(24)
#define FNAV_TYPE2_TOW_LAST_BYTE_IDX 			(26)
#define FNAV_TYPE2_TOW_BITS_OF_FIRST_BYTE 		(6)
#define FNAV_TYPE2_TOW_BITS_OF_LAST_BYTE 		(6)

#define FNAV_TYPE3_IOD_FIRST_BYTE_IDX 			(0)
#define FNAV_TYPE3_IOD_LAST_BYTE_IDX 			(1)
#define FNAV_TYPE3_IOD_BITS_OF_FIRST_BYTE 		(2)
#define FNAV_TYPE3_IOD_BITS_OF_LAST_BYTE 		(8)

#define FNAV_TYPE3_WN_FIRST_BYTE_IDX 			(21)
#define FNAV_TYPE3_WN_LAST_BYTE_IDX 			(23)
#define FNAV_TYPE3_WN_BITS_OF_FIRST_BYTE 		(2)
#define FNAV_TYPE3_WN_BITS_OF_LAST_BYTE 		(2)

#define FNAV_TYPE3_TOW_FIRST_BYTE_IDX 			(23)
#define FNAV_TYPE3_TOW_LAST_BYTE_IDX 			(25)
#define FNAV_TYPE3_TOW_BITS_OF_FIRST_BYTE 		(6)
#define FNAV_TYPE3_TOW_BITS_OF_LAST_BYTE 		(6)

#define FNAV_TYPE4_IOD_FIRST_BYTE_IDX 			(0)
#define FNAV_TYPE4_IOD_LAST_BYTE_IDX 			(1)
#define FNAV_TYPE4_IOD_BITS_OF_FIRST_BYTE 		(2)
#define FNAV_TYPE4_IOD_BITS_OF_LAST_BYTE 		(8)

#define FNAV_TYPE4_TOW_FIRST_BYTE_IDX 			(23)
#define FNAV_TYPE4_TOW_LAST_BYTE_IDX 			(26)
#define FNAV_TYPE4_TOW_BITS_OF_FIRST_BYTE 		(3)
#define FNAV_TYPE4_TOW_BITS_OF_LAST_BYTE 		(1)

#define FNAV_TYPE5_IODA_FIRST_BYTE_IDX 			(0)
#define FNAV_TYPE5_IODA_LAST_BYTE_IDX 			(1)
#define FNAV_TYPE5_IODA_BITS_OF_FIRST_BYTE 		(2)
#define FNAV_TYPE5_IODA_BITS_OF_LAST_BYTE 		(2)

#define FNAV_TYPE5_SVID1_FIRST_BYTE_IDX			(2)
#define FNAV_TYPE5_SVID1_LAST_BYTE_IDX 			(3)
#define FNAV_TYPE5_SVID1_BITS_OF_FIRST_BYTE		(2)
#define FNAV_TYPE5_SVID1_BITS_OF_LAST_BYTE 		(4)

#define FNAV_TYPE5_SVID2_FIRST_BYTE_IDX			(19)
#define FNAV_TYPE5_SVID2_LAST_BYTE_IDX 			(19)
#define FNAV_TYPE5_SVID2_BITS_OF_FIRST_BYTE		(7)
#define FNAV_TYPE5_SVID2_BITS_OF_LAST_BYTE 		(7)

#define FNAV_TYPE6_IODA_FIRST_BYTE_IDX 			(0)
#define FNAV_TYPE6_IODA_LAST_BYTE_IDX 			(1)
#define FNAV_TYPE6_IODA_BITS_OF_FIRST_BYTE 		(2)
#define FNAV_TYPE6_IODA_BITS_OF_LAST_BYTE 		(2)

#define FNAV_TYPE6_SVID3_FIRST_BYTE_IDX			(10)
#define FNAV_TYPE6_SVID3_LAST_BYTE_IDX 			(10)
#define FNAV_TYPE6_SVID3_BITS_OF_FIRST_BYTE		(6)
#define FNAV_TYPE6_SVID3_BITS_OF_LAST_BYTE 		(0)


#endif /* FNAV_MESSAGE_PARSER_H_INCLUDED */
