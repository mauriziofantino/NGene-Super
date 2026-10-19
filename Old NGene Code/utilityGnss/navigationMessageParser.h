/**
 * @file navigationMessageParser.h
 *
 * @author Mario Nicola
 *
 * @date 06/10/2014
 *
 * @version 1.0.0
 *
 * @brief Some basic functions to manage the parse of navigation message.
 */

#ifndef NAVIGATION_MESSAGE_PARSER_H_INCLUDED
#define NAVIGATION_MESSAGE_PARSER_H_INCLUDED

#include <stdio.h>
#include "navigationMessageDataStructure.h"
#include "osnmaDataStructures.h"
#include "pvtConfiguration.h"


/**
 * @enum NavigationMessageParseStatusEnum
 *
 * @brief This is the type that indicates the state of the parsing for the current channel.
 */
typedef enum NavigationMessageParseStatusEnum
{
	SEARCH_SYNCHRONIZATION,		/**< The subframe/page synchronization has not been found */
	SEARCH_SYNCHRONIZATION_2,	/**< The subframe/page synchronization has not been found (EGNOS only) */
	FIRST_DECODING,				/**< The first subframe/page must be still decoded */
	PREAMBLE_1,					/**< Must receive the preamble of the even page part (Galileo only) */
	DECODING_1,					/**< The subframe/even page part is under decoding */
	PREAMBLE_2,					/**< Must receive the preamble of the odd page part (Galileo only) */
	DECODING_2					/**< The odd page part is under decoding (Galileo only) */
} TNavigationMessageParseStatus;

/**
 * @enum ParseResultEnum
 *
 * @brief This is the type that indicates the return value of the functions that parse navigation message.
 */
typedef enum ParseResultEnum
{
	PREAMBLE_NOT_FOUND,		/**< The subframe/page preamble has not been found */
	PREAMBLE_FOUND,			/**< The subframe/page preamble has been found */
	CRC_ERROR,				/**< A CRC error occurred during subframe/page decoding */
	DECODING_ON_GOING,		/**< Not enough have been collected to proceed with the subframe/page decoding */
	DECODING_ERROR, 		/**< A generic error occurred during subframe/page decoding (e.g. out of range value) */
	DECODING_1_SUCCESSFUL,	/**< The subframe/even page part has been decoded successfully */
	DECODING_2_SUCCESSFUL	/**< The odd page part has been decoded successfully */
} TParseResult;

/* GPS */
/**
 * @def GPS_PREAMBLE_BITS
 * @brief First 8 bits of a GPS subframe are 10001011
 */
#define GPS_PREAMBLE_BITS 		0x8b

/**
 * @def GPS_INV_PREAMBLE_BITS
 * @brief When a pi phase rotation happens,
 *        first 8 bits of a GPS subframe are 01110100
 */
#define GPS_INV_PREAMBLE_BITS	0x74

/**
 * @def GPS_PREAMBLE_LENGTH
 * @brief Length of the GPS subframe preamble
 */
#define GPS_PREAMBLE_LENGTH 	8


/**
 * @def	GPS_WORD_LENGTH
 * @brief Each GPS word is made of 30 bits
 */
#define GPS_WORD_LENGTH		30


/**
 * @def	GPS_DATA_BITS_PER_WORD
 * @brief Each GPS word is made of 24 data bits and 6 parity bits
 */
#define GPS_DATA_BITS_PER_WORD		24


/**
 * @def GPS_SUBFRAME_LENGTH
 * @brief Each GPS subframe is made of GPS_SUBFRAME_LENGTH words (300 bits)
 */
#define GPS_SUBFRAME_LENGTH		10

/**
 * @def GPS_SUBFRAME_BYTES
 * @brief Each GPS subframe can be stored in GPS_SUBFRAME_BYTES bytes
 */
#define GPS_SUBFRAME_BYTES		40

/**
 * @def GPS_SUBFRAME_DURATION
 * @bried Duration of a GPS subframe [s]
 */
#define GPS_SUBFRAME_DURATION	6

/**
 * @def GPS_SUBFRAMES_PER_FRAME
 * @brief How many subframes in one frame of the GPS L1 C/A navigation message
 */
#define GPS_SUBFRAMES_PER_FRAME  5

/**
 * @def GPS_FRAME_BYTES
 * @brief Length of a GPS frame in bytes
 */
#define GPS_FRAME_BYTES  (GPS_SUBFRAMES_PER_FRAME*GPS_SUBFRAME_BYTES)

/**
 * @def GPS_FRAME_DURATION
 * @bried Duration of a GPS frame [s]
 */
#define GPS_FRAME_DURATION  (GPS_SUBFRAMES_PER_FRAME*GPS_SUBFRAME_DURATION)

/**
 * @struct GpsParserStruct
 *
 * @brief This structure contains variables used to parse GPS navigation message.
 */
typedef struct GpsParserStruct
{
	unsigned int bitIndexTotal;					/**< Number of received bits after the current TOW */
	unsigned int lastWord;						/**< Last received word of the GPS navigation message */
	int bitIndex;								/**< Number of valid bits in the last received word */
	unsigned int wordIndex;						/**< Index of the last received word in the current GPS subframe */
	unsigned char subframe[GPS_SUBFRAME_BYTES];	/**< The content of the subframe to be parsed */
	unsigned int crcPassed;						/**< Number of consecutive words with correct parity bits. Reset when:
	 	 	 	 	 	 	 	 	 	 	 	 	 - a CRC error occurs
	 	 	 	 	 	 	 	 	 	 	 	 	 - a subframe is successfully parsed */
	int wnCorrection;                           /**< add to correct WN rollover */
	TNavigationMessageParseStatus status;		/**< Indicates the status of the parse of the GPS navigation message  */
	TNavigationMessage *navMsg;					/**< Points to the structure storing the navigation message for the current PRN */
	// osnma
	unsigned char frame[GPS_FRAME_BYTES];       /**< Stores subframes 1,2,3,4,5 for the cross-authentication with Galileo OSNMA */
	int lastSavedSubframe;                      /**< Last subframe stored in the frame variable (1,2,3,4,5) */
	// log
	FILE *gpsParseLogFile;						/**< The content of the parse navigation message is saved in a text file for debug */
} TGpsParser;


/* GALILEO */

/**
 * @def GALILEO_PAGE_PART_EVEN
 * @brief Value of the field in the Galileo navigation message that indicates whenever
 *        the page part is even.
 */
#define	GALILEO_PAGE_PART_EVEN 0

/**
 * @def GALILEO_PAGE_PART_ODD
 * @brief Value of the field in the Galileo navigation message that indicates whenever
 *        the page part is odd.
 */
#define GALILEO_PAGE_PART_ODD  1

/**
 * @def GALILEO_NOMINAL_PAGE
 * @brief Value of the field in the Galileo navigation message that indicates whenever
 *        the page part is a nominal page.
 */
#define GALILEO_NOMINAL_PAGE 0

/**
 * @def GALILEO_ALERT_PAGE
 * @brief Value of the field in the Galileo navigation message that indicates whenever
 *        the page part is an alert page (not parsed).
 */
#define GALILEO_ALERT_PAGE 1

/**
 * @def GAL_PREAMBLE_BITS
 * @brief First 10 bits of a Galileo page part are 0101100000
 */
#define GAL_PREAMBLE_BITS 		0x0160

/**
 * @def GAL_INV_PREAMBLE_BITS
 * @brief When a pi phase rotation happens,
 *        first 10 bits of a Galileo page part are 1010011111
 */
#define GAL_INV_PREAMBLE_BITS	0x029f

/**
 * @def GAL_PREAMBLE_LENGTH
 * @brief Length of the Galileo page part preamble
 */
#define GAL_PREAMBLE_LENGTH 	10

/**
 * @def GAL_PAGE_LENGTH
 * @brief Each Galileo page (preamble included) is made of 500 symbols
 */
#define GAL_PAGE_LENGTH 	500

/**
 * @def GAL_PAGE_PART_LENGTH
 * @brief Each Galileo page part (preamble included) is made of 250 symbols
 */
#define GAL_PAGE_PART_LENGTH 	250

/**
 * @def GAL_PAGE_DURATION
 * @brief Each Galileo page lasts 2 seconds
 */
#define GAL_PAGE_DURATION  2

/**
 * @def GAL_PAGE_PART_DURATION
 * @brief Each Galileo page part lasts 1 second
 */
#define GAL_PAGE_PART_DURATION  1

/**
 * @def GAL_ENCODED_PAGE_PART_LENGTH_SYMBOLS
 * @brief Each Galileo page part (preamble excluded) is made of 240 symbols
 */
#define GAL_ENCODED_PAGE_PART_LENGTH_SYMBOLS	240

/**
 * @def GAL_ENCODED_PAGE_PART_LENGTH_BYTES
 * @brief Each Galileo page part (preamble excluded) can be stored in 30 bytes
 */
#define GAL_ENCODED_PAGE_PART_LENGTH_BYTES	30

/**
 * @def GAL_DECODED_PAGE_PART_LENGTH_BYTES
 * @brief Each Galileo page part (preamble excluded) AFTER Viterbi decoding
 * 		  can be stored in 15 bytes (convolutional code with rate 1/2!)
 */
#define GAL_DECODED_PAGE_PART_LENGTH_BYTES	15

/**
 * @def GAL_DECODED_PAGE_PART_LENGTH_BITS
 * @brief Each Galileo page part (preamble excluded) AFTER Viterbi decoding
 * 		  can be stored in 120 bits (convolutional code with rate 1/2!)
 */
#define GAL_DECODED_PAGE_PART_LENGTH_BITS	120

/**
 * @def GAL_ARRAY_FOR_CRC_LENGTH_BITS
 * @brief Length of the array where are stored the bits for the CRC control [bits]
 */
#define GAL_ARRAY_FOR_CRC_LENGTH_BITS	220

/**
 * @def GAL_ARRAY_FOR_CRC_LENGTH_BYTES
 * @brief Length of the array where are stored the bits for the CRC control [bytes]
 *        Takes into account that last byte is not full.
 *        IMPORTANT WARNING: It includes also the CRC bits!!!
 */
#define GAL_ARRAY_FOR_CRC_LENGTH_BYTES	28

/**
 * @def GAL_ARRAY_FOR_PARSE_LENGTH_BITS
 * @brief Length of the array where are stored the bits for the parse [bits]
 */
#define GAL_ARRAY_FOR_PARSE_LENGTH_BITS		128

/**
 * @def GAL_ARRAY_FOR_PARSE_LENGTH_BYTES
 * @brief Length of the array where are stored the bits for the parse [bytes]
 */
#define GAL_ARRAY_FOR_PARSE_LENGTH_BYTES	16

/**
 * @def GAL_RESERVED1_SIZE_BITS
 * @brief  Size of "Reserved 1" field in odd part of I/NAV [bits]
 */
#define GAL_RESERVED1_SIZE_BITS  40

/**
 * @def GAL_RESERVED1_SIZE_BYTES
 * @brief  Size of "Reserved 1" field in odd part of I/NAV [bytes]
 */
#define GAL_RESERVED1_SIZE_BYTES  5

/**
 * @def GAL_SSP1
 * @brief  Value of the first valid I/NAV SSP
 */
#define GAL_SSP1  0x04

/**
 * @def GAL_SSP2
 * @brief  Value of the second valid I/NAV SSP
 */
#define GAL_SSP2  0x2b

/**
 * @def GAL_SSP3
 * @brief  Value of the third valid I/NAV SSP
 */
#define GAL_SSP3  0x2f

/**
 * @def GAL_RS_INFORMATION_VECTOR_LENGTH_BYTES
 * @brief The information vector for the Reed-Solomon FEC2 is made of
 *        16 bytes from Word 1, 14 bytes from each words 2, 3, and 4.
 */
#define GAL_RS_INFORMATION_VECTOR_LENGTH_BYTES	(16+14+14+14)

/**
 * @def GAL_RS_PARITY_VECTOR_LENGTH_BYTES
 * @bried The parity vector for the Reed-Solomon FEC2 is made of
 *        15 bytes from each word 17, 18, 19, and 20
 */
#define GAL_RS_PARITY_VECTOR_LENGTH_BYTES       (4*15)

/**
 * @def GAL_RS_NUM_OF_USED_WORDS
 * @brief The Reed-Solomon FEC2 uses 4 words for the information vectors
 *        and 4 words for the parity vector.
 */
#define GAL_RS_NUM_OF_USED_WORDS   4


/**
 * @struct GalParserStruct
 *
 * @brief This structure contains variables used to parse Galileo navigation message.
 */
typedef struct GalParserStruct
{
	int satId;
	unsigned char galPreamble[2];				/**< Galileo preamble stored in unsigned char */
	unsigned char galPreambleInv[2];			/**< Galileo preamble stored in unsigned char */
	unsigned char polarityCorrection;
	unsigned int bitIndexTotal;					/**< Number of received bits after the current TOW */
	unsigned char pagePart[GAL_ENCODED_PAGE_PART_LENGTH_BYTES];
	int byteIndex;								/**< Index of the byte in the last received page part */
	int bitIndex;								/**< Number of valid bits in the last received page part */
	unsigned char deinterleavedPagePart[GAL_ENCODED_PAGE_PART_LENGTH_SYMBOLS];
	unsigned char decodedPagePart1[GAL_DECODED_PAGE_PART_LENGTH_BYTES];
	unsigned char decodedPagePart2[GAL_DECODED_PAGE_PART_LENGTH_BYTES];
	unsigned char bitsForCrc[GAL_ARRAY_FOR_CRC_LENGTH_BYTES];
	unsigned char bitsForParse[GAL_ARRAY_FOR_PARSE_LENGTH_BYTES];
	int synchronizationSuccess; 				/**< The page synchronization has been successful when
	                                                 - the preamble has been found
	                                                 - the following page has been decoded without CRC errors */
	int dummyMsg;                               /**< If TRUE_FLAG, the message is made of DUMMY words; FALSE_FLAG elsewhere.
	                                                 For output purposes. */
	TRedCedInPvt redCedInPvt;                   /**< It replicates the PVT configuration parameter to manage RedCED parse accordingly */
	int wnCorrection;                           /**< add to correct WN rollover */
	TNavigationMessageParseStatus status;		/**< Indicates the status of the parse of the Galileo navigation message  */
	TNavigationMessage *navMsg;					/**< Points to the structure storing the navigation message for the current PRN */
	/* almanac decoding */
	int almSvIdPrev;							/**< SVID of the satellite whose almanac is in reception */
	int almSvIdPrevTime;                        /**< time when the first part of the almanac in reception is arrived */

	/* spare and reserved fields (to support OS-NMA) */
	unsigned char reserved1[GAL_RESERVED1_SIZE_BYTES];    /**< Reserved 1 field packed: first bit in MS bit of byte 0, and so on ... */
	unsigned char reserved1Unpk[GAL_RESERVED1_SIZE_BITS]; /**< Reserved 1 field unpacked: first bit in byte 0, and so on ... */

	TChannelOsnmaVariables chOsnma; /**< used to parse OSNMA bits */

	/* last SSP (Secondary Synchronization Pattern) */
	unsigned char sspIndex;  /**< index of the last received SSP: valid values: 1,2,3; 0 is invalid */
	unsigned char ssp;       /**< last received SSP (after Viterbi decoding) */

	/* Reed-Solomon FEC2 */
	int rsIod; /**< Two LSB of the IODnav for the RS */
	// flags for RS management
	// received FALSE, valid FALSE => word not received
	// received TRUE, valid FALSE => word received, CRC error occurred
	// received TRUE, valid TRUE => word received , valid CRC
	// received FALSE, valid TRUE => should not happen
	int rsInformationReceived[GAL_RS_NUM_OF_USED_WORDS]; /**< One received flag for each part of the information vector */
	int rsInformationValid[GAL_RS_NUM_OF_USED_WORDS]; /**< One validity flag for each part of the information vector */
	unsigned char rsInformationVector[GAL_RS_INFORMATION_VECTOR_LENGTH_BYTES]; /**< */
	int rsParityReceived[GAL_RS_NUM_OF_USED_WORDS]; /**< One received flag for each part of the parity vector */
	int rsParityValid[GAL_RS_NUM_OF_USED_WORDS]; /**< One validity flag for each part of the parity vector */
	unsigned char rsParityVector[GAL_RS_PARITY_VECTOR_LENGTH_BYTES]; /**< */
	
	/* log */
	FILE *galParseLogFile;						/**< The content of the parse navigation message is saved in a text file for debug */
} TGalParser;


/* F/NAV */
/**
 * @def FNAV_PREAMBLE_BITS
 * @brief First 12 bits of a Galileo F/NAV page are 1011 0111 0000
 */
#define FNAV_PREAMBLE_BITS 		0x0b70

/**
 * @def FNAV_INV_PREAMBLE_BITS
 * @brief When a pi phase rotation happens,
 *        first 12 bits of a Galileo F/NAV page are 0100 1000 1111
 */
#define FNAV_INV_PREAMBLE_BITS	0x048f

/**
 * @def FNAV_PREAMBLE_LENGTH
 * @brief Length of the Galileo F/NAV page preamble
 */
#define FNAV_PREAMBLE_LENGTH 	12

/**
 * @def FNAV_PAGE_LENGTH
 * @brief Each Galileo F/NAV page (preamble included) is made of 500 symbols
 */
#define FNAV_PAGE_LENGTH 	500

/**
 * @def FNAV_PAGE_DURATION
 * @brief Each Galileo F/NAV page lasts 10 seconds
 */
#define FNAV_PAGE_DURATION  10

/**
 * @def FNAV_ENCODED_PAGE_LENGTH_SYMBOLS
 * @brief Each Galileo F/NAV page (preamble excluded) is made of 488 symbols
 */
#define FNAV_ENCODED_PAGE_LENGTH_SYMBOLS	488

/**
 * @def FNAV_ENCODED_PAGE_LENGTH_BYTES
 * @brief Each Galileo F/NAV page (preamble excluded) can be stored in 61 bytes
 */
#define FNAV_ENCODED_PAGE_LENGTH_BYTES	61

/**
 * @def FNAV_DECODED_PAGE_LENGTH_BYTES
 * @brief Each Galileo F/NAV page (preamble excluded) AFTER Viterbi decoding
 * 		  can be stored in 31 bytes (convolutional code with rate 1/2!).
 * 		  A four bits padding must be added to fill the last byte.
 */
#define FNAV_DECODED_PAGE_LENGTH_BYTES	31

/**
 * @def FNAV_DECODED_PAGE_LENGTH_BITS
 * @brief Each Galileo F/NAV page (preamble excluded) AFTER Viterbi decoding
 * 		  can be stored in 244 bits (convolutional code with rate 1/2!)
 */
#define FNAV_DECODED_PAGE_LENGTH_BITS	244

/**
 * @def FNAV_ARRAY_FOR_CRC_LENGTH_BITS
 * @brief Length of the array where are stored the bits for the CRC control [bits]
 */
#define FNAV_ARRAY_FOR_CRC_LENGTH_BITS	214

/**
 * @def FNAV_ARRAY_FOR_CRC_LENGTH_BYTES
 * @brief Length of the array where are stored the bits for the CRC control [bytes]
 *        Takes into account that last byte is not full.
 *        IMPORTANT WARNING: It includes also the CRC bits!!!
 */
#define FNAV_ARRAY_FOR_CRC_LENGTH_BYTES	30

/**
 * @def FNAV_PAGES_PER_SUBFRAME
 * @brief An F/NAV subframe is made of 5 pages.
 */
#define FNAV_PAGES_PER_SUBFRAME 5

/**
 * @def FNAV_SUBFRAME_BYTES
 * @brief Length in bytes of an F/NAV subframe.
 *        It includes four padding bits at the end of each page!!!
 */
#define FNAV_SUBFRAME_BYTES	(FNAV_DECODED_PAGE_LENGTH_BYTES*FNAV_PAGES_PER_SUBFRAME)

/**
 * @def FNAV_SUBFRAME_DURATION
 * @brief Duration of an F/NAV subframe [s].
 */
#define FNAV_SUBFRAME_DURATION	(FNAV_PAGES_PER_SUBFRAME*FNAV_PAGE_DURATION)





/**
 * @struct FNavParserStruct
 *
 * @brief This structure contains variables used to parse Galileo F/NAV navigation message.
 */
typedef struct FNavParserStruct
{
	int satId;
	unsigned char fnavPreamble[2];				/**< Galileo F/NAV preamble stored in unsigned char */
	unsigned char fnavPreambleInv[2];			/**< Galileo F/NAV preamble stored in unsigned char */
	unsigned char polarityCorrection;
	unsigned int bitIndexTotal;					/**< Number of received bits after the current TOW */
	unsigned char page[FNAV_ENCODED_PAGE_LENGTH_BYTES];
	int byteIndex;								/**< Index of the byte in the last received page part */
	int bitIndex;								/**< Number of valid bits in the last received page part */
	unsigned char deinterleavedPage[FNAV_ENCODED_PAGE_LENGTH_SYMBOLS];
	unsigned char decodedPage[FNAV_DECODED_PAGE_LENGTH_BYTES];
	int synchronizationSuccess; 				/**< The page synchronization has been successful when
	                                                 - the preamble has been found
	                                                 - the following page has been decoded without CRC errors */
	int dummyMsg;                               /**< If TRUE_FLAG, the message is made of DUMMY words; FALSE_FLAG elsewhere.
	                                                 For output purposes. */
	int wnCorrection;                           /**< add to correct WN rollover */
	TNavigationMessageParseStatus status;		/**< Indicates the status of the parse of the Galileo FNAV navigation message  */
	TNavigationMessage *navMsg;					/**< Points to the structure storing the navigation message for the current PRN */
	// osnma
	unsigned char subframe[FNAV_SUBFRAME_BYTES];/**< Stores pages 1,2,3,4,5 for the cross-authentication with Galileo OSNMA */
	int lastSavedPage; 		                    /**< Last page stored in the subframe variable (1,2,3,4,5) */
	// log
	FILE *fnavParseLogFile;						/**< The content of the parse navigation message is saved in a text file for debug */
} TFNavParser;


/* EGNOS */

#define EGNOS_ENCODED_BLOCK_LENGTH_SYMBOLS		500
#define EGNOS_DECODED_BLOCK_LENGTH_BITS			250
#define EGNOS_ADDITIONAL_BLOCK_LENGTH_SYMBOLS	500
#define EGNOS_ADDITIONAL_BLOCK_LENGTH_BITS		250
#define EGNOS_ARRAY_FOR_PARSE_LENGTH_BYTES		 32
#define EGNOS_PREAMBLE_1 		(0x53)
#define EGNOS_PREAMBLE_INV_1 	(0xAC)
#define EGNOS_PREAMBLE_2 		(0x9A)
#define EGNOS_PREAMBLE_INV_2 	(0x65)
#define EGNOS_PREAMBLE_3 		(0xC6)
#define EGNOS_PREAMBLE_INV_3 	(0x39)

/**
 * @enum EgnosPreambleIndexEnum
 * @brief EGNOS message has three preamble alternated in the navigation message blocks.
 */
typedef enum EgnosPreambleIndexEnum
{
	EGNOS_PREAMBLE_INVALID_INDEX=-1,/**< The value is invalid */
	EGNOS_PREAMBLE_1_INDEX=0,		/**< Index for the EGNOS_PREAMBLE_1 */
	EGNOS_PREAMBLE_2_INDEX=1,		/**< Index for the EGNOS_PREAMBLE_2 */
	EGNOS_PREAMBLE_3_INDEX=2		/**< Index for the EGNOS_PREAMBLE_3 */
} TEgnosPreambleIndex;

/**
 * @struct EgnosParserStruct
 *
 * @brief This structure contains variables used to parse EGNOS navigation message.
 */
typedef struct EgnosParserStruct
{
	unsigned char symbols[EGNOS_ENCODED_BLOCK_LENGTH_SYMBOLS+EGNOS_ADDITIONAL_BLOCK_LENGTH_SYMBOLS+1];		/**< */
	unsigned int symbolIndex;
	unsigned char polarityCorrection;
	unsigned long path1[2*EGNOS_DECODED_BLOCK_LENGTH_BITS];
	unsigned long path2[2*EGNOS_DECODED_BLOCK_LENGTH_BITS];
	long stateMetric[64];
	TNavigationMessageParseStatus status;
	TEgnosPreambleIndex expectedPreamble;
	unsigned char decodedBits[EGNOS_ARRAY_FOR_PARSE_LENGTH_BYTES];
	unsigned char additionalBits[EGNOS_ARRAY_FOR_PARSE_LENGTH_BYTES];
	// logs
	FILE *egnosParseLogFile;						/**< The content of the parsed navigation message is saved in a text file for debug */
	FILE *egnosBitsLogFile; 						/**< The output of the Viterbi, after the synchronization, is saved in a text file for debug */
} TEgnosParser;


/*
 * Hereafter the functions used to parse each field in the navigation message
 * are reported. This comment explains the meaning of the inputs: for each input
 * a proper define will be used.
 *
 * Bits of the navigation message are stored in a char array (unsigned int for GPS),
 * where 8 bits are packed in each byte, starting from the most significant
 * bit of the byte with index 0. These defines allow the receiver to locate
 * each field in the array. For each field four defines are needed:
 * 1) Index of the first byte containing the field
 * 2) Index of the last byte containing the field
 * 3) Bits in the first byte
 * 4) Bits in the last byte
 *
 * In order to understand which value must be assigned to each define,
 * a list of the different possible conditions is included
 * A) and B) the field occupies several bytes
 * C) the field occupies just some of the less significant bits of one byte
 * D) the field occupies just some of the most significant bits of one byte
 * E) the field occupies some bits in the middle of one byte
 * An example of these conditions are graphically reported in the following
 * (where the "X"s denote the bits that will be extracted from the array,
 *  "o"s denotes bits that must be discarded):
 *
 *   | byte 0 | byte 1 | byte 2 | byte 3 | byte 4 |...
 *   |01234567|01234567|01234567|01234567|01234567|...
 *   -------------------------------------------------
 * A |ooooooXX|XXXXXXXX|Xooooooo|oooooooo|oooooooo|...
 *   -------------------------------------------------
 * B |ooooooXX|XXXXXXXX|oooooooo|oooooooo|oooooooo|...
 *   -------------------------------------------------
 * C |ooooooXX|oooooooo|oooooooo|oooooooo|oooooooo|...
 *   -------------------------------------------------
 * D |oooooooo|XXXooooo|oooooooo|oooooooo|oooooooo|...
 *   -------------------------------------------------
 * E |oooooooo|oooooXXo|oooooooo|oooooooo|oooooooo|...
 *   -------------------------------------------------
 *
 * In the depicted case A
 *   FIRST_BYTE_IDX = 0
 *   LAST_BYTE_IDX = 2
 *   BITS_OF_FIRST_BYTE = 2
 *   BITS_OF_LAST_BYTE = 1
 *
 * In the depicted case B
 *   FIRST_BYTE_IDX = 0
 *   LAST_BYTE_IDX = 1
 *   BITS_OF_FIRST_BYTE = 2
 *   BITS_OF_LAST_BYTE = 8
 *
 * In the depicted case C
 *   FIRST_BYTE_IDX = 0
 *   LAST_BYTE_IDX = 0
 *   BITS_OF_FIRST_BYTE = 0
 *   BITS_OF_LAST_BYTE = 2
 *
 * In the depicted case D
 *   FIRST_BYTE_IDX = 1
 *   LAST_BYTE_IDX = 1
 *   BITS_OF_FIRST_BYTE = 3
 *   BITS_OF_LAST_BYTE = 0
 *
 * In the depicted case E
 *   FIRST_BYTE_IDX = 1
 *   LAST_BYTE_IDX = 1
 *   BITS_OF_FIRST_BYTE = 7
 *   BITS_OF_LAST_BYTE = 3
 *
 *   (1234567
 *    oooooXXo
 *         123)
 *
 * These values are compatible with the implemented parser function.
 */

/**
 * @brief ExtractTwoCompField parses an array of bits and extracts a signed integer.
 * @details The detailed explanation of the indexes is shown up here.
 *
 * @param dataBytes array containing the bits of the navigation message
 * @param startByte is the index of the byte containing the most significant bit
 * @param endByte is the index of the byte containing the least significant bit
 * @param bitsOfFirstByte is the number of the bits in the start byte
 * @param bitsOfLastByte is the number of the bits in the end byte
 * @param numBits number of bits to be parsed
 *
 * @return The parsed value
 */
double ExtractTwoCompField(unsigned char dataBytes[], int startByte, int endByte, int bitsOfFirstByte, int bitsOfLastByte, int numBits);

/**
 * @brief ExtractField parses an array of bits and extracts an unsigned integer.
 * @details The detailed explanation of the indexes is shown up here.
 *
 * @param dataBytes array containing the bits of the navigation message
 * @param startByte is the index of the byte containing the most significant bit
 * @param endByte is the index of the byte containing the least significant bit
 * @param bitsOfFirstByte is the number of the bits in the start byte
 * @param bitsOfLastByte is the number of the bits in the end byte
 *
 * @return The parsed value
 */
double ExtractField(unsigned char dataBytes[], int startByte, int endByte, int bitsOfFirstByte, int bitsOfLastByte);


/**
 * @brief ExtractBits extracts specified bits from an array of bytes.
 * @details The first bit is saved in the MS bit of the first output byte (index 0).
 *          If the number of selected bits is not a multiple of 8, the remaining bits
 *          of the last output byte will be set to zero.
 *          The detailed explanation of the indexes is shown up here.
 *
 * @param startByte is the index of the byte containing the most significant bit
 * @param endByte is the index of the byte containing the least significant bit
 * @param bitsOfFirstByte is the number of the bits in the start byte
 * @param bitsOfLastByte is the number of the bits in the end byte
 * @param inputBytes is the array of bytes where the bits are read from
 * @param outputBytes is the array of bytes where the bits are written into
 */
void ExtractBits(int startByte, int endByte, int bitsOfFirstByte, int bitsOfLastByte, unsigned char inputBytes[], unsigned char outputBytes[]);


/**
 * @brief CopyBitsUnaligned copies specified bits from an array of bytes.
 *        Output indexes (both byte and bit) are updated: at the end of
 *        the copy, they point to the next bit.
 * @details Differently from ExtractBits, both input and output arrays can start from a bit
 *          which is not the MS one. Bit indexing is reported as follows:
 *           MS                           LS
 *          | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
 *
 * @param byteIdxIn is the byte index of the first input bit
 * @param bitIdxIn is the bit index of the first input bit
 * @param arrayIn points to the input array
 * @param byteIdxOutPtr points to the byte index of the first output bit
 * @param bitIdxOutPtr points to the bit index of the first output bit
 * @param arrayOut points to the output array
 * @param len is the number of bits to be copied
 */
void CopyBitsUnaligned(int byteIdxIn,int bitIdxIn,unsigned char *arrayIn,int *byteIdxOutPtr,int *bitIdxOutPtr,unsigned char *arrayOut,int len);


#define POW_TWO_M02             (0.250000000000000)
#define POW_TWO_M04             (0.062500000000000)
#define POW_TWO_M05 			(3.125000000000000e-02)
#define POW_TWO_M08             (3.906250000000000e-03)
#define POW_TWO_M09				(1.953125000000000e-03)
#define POW_TWO_M11 			(4.882812500000000e-04)
#define POW_TWO_M12 			(2.441406250000000e-04)
#define POW_TWO_M13 			(1.220703125000000e-04)
#define POW_TWO_M14				(6.103500000000000e-05)
#define POW_TWO_M15 			(3.051800000000000e-05)
#define POW_TWO_M16 			(1.525900000000000e-05)
#define POW_TWO_M19 			(1.907348632812500e-06)
#define POW_TWO_M20 			(9.536743164062500e-07)
#define POW_TWO_M21 			(4.768371582031250e-07)
#define POW_TWO_M22             (2.384185791015625e-07)
#define POW_TWO_M23 			(1.192092895507812e-07)
#define POW_TWO_M24 			(5.960464477539062e-08)
#define POW_TWO_M26 			(1.490116119384766e-08)
#define POW_TWO_M27 			(7.450580596923828e-09)
#define POW_TWO_M29 			(1.862645149230957e-09)
#define POW_TWO_M30 			(9.313225746154785e-10)
#define POW_TWO_M31 			(4.656612873077393e-10)
#define POW_TWO_M32 			(2.328306436538696e-10)
#define POW_TWO_M33 			(1.164153218269348e-10)
#define POW_TWO_M34 			(5.820766091346741e-11)
#define POW_TWO_M35 			(2.910383045673370e-11)
#define POW_TWO_M37 			(7.275957614183426e-12)
#define POW_TWO_M38 			(3.637978807091713e-12)
#define POW_TWO_M39 			(1.818989403545857e-12)
#define POW_TWO_M43 			(1.136868377216160e-13)
#define POW_TWO_M46 			(1.421085471520200e-14)
#define POW_TWO_M50 			(8.881784197001252e-16)
#define POW_TWO_M51 			(4.440892098500626e-16)
#define POW_TWO_M55 			(2.775557561562891e-17)
#define POW_TWO_M59 			(1.734723475976807e-18)
#define POW_TWO_P03				(8)
#define POW_TWO_P08				(256)
#define POW_TWO_P11  			(2048)
#define POW_TWO_P12  			(4096)
#define POW_TWO_P14 			(16384)
#define POW_TWO_P16 			(65536)
#define POW_TWO_P24				(16777216.0)


#endif /* NAVIGATION_MESSAGE_PARSER_H_INCLUDED */
