/**
 * @file navigationMessageParser.c
 *
 * @author Mario Nicola
 *
 * @date 06/10/2014
 *
 * @version 1.0.0
 *
 * @brief Some basic functions to manage the parse of navigation message.
 */
#include <math.h>

double ExtractTwoCompField(unsigned char dataBytes[], int startByte, int endByte, int bitsOfFirstByte, int bitsOfLastByte, int numBits)
{
	int msByteShift;
	int lsByteShift;
	unsigned int temp;
	double res;

	/* number of bits of dataBits[startByte] to shift */
	msByteShift = 8 - bitsOfFirstByte;
	/* number of bits of dataBits[endByte] to shift */
	lsByteShift = 8 - bitsOfLastByte;

	/* the bits of the field to extract are on 1 dataBits element */
	if(startByte==endByte)
	{
		/* take the less-significant bitsOfLastByte bits */
		if(msByteShift==8)
		{
			temp = dataBytes[startByte];
			temp = (temp << (24 + lsByteShift)) >> (24 + lsByteShift);
			res = temp;
		}
		/* take the most-significant bitsOfFirstByte bits */
		else
		{
			res = dataBytes[startByte] >> msByteShift;
		}
	}
	/* the bits of the field to extract are on several (endByte-startByte+1) consecutive dataBits element */
	else
	{
		temp = dataBytes[startByte];
		/* take the bitsOfFirstByte less-significant bits of the dataBits[startByte] and copy in a integer of 32 bit variable (temp) */
		if(msByteShift!=0)
		{
			temp = (temp << (24 + msByteShift)) >> (24 + msByteShift);
		}
		/* left shift to give space to the consecutive bytes */
		temp = ( temp << ((8 * (endByte-startByte))-lsByteShift) );
		/* assignment to the final result */
		res = temp;
		/*
		 * if present, take the full bytes in the middle (between dataBits[startByte] and dataBits[endByte]) and put them in the right
		 * position inside the 32 bit variable (temp)
		 */
		int i;
		for(i=startByte+1;i<endByte;i++)
		{
			temp = dataBytes[i];
			temp = ( temp << ((8 * (endByte-i))-lsByteShift) );
			res += temp;
		}
		/* take the bitsOfLastByte most-significant bit inside dataBits[endByte] */
		temp = dataBytes[endByte];
		temp = temp >> lsByteShift;
		// sum to the final result
		res += temp;
	}
	/* calculate twos complement */
	if((pow(2,numBits-1)<=res) && (res<=pow(2,numBits)-1))
	{
		res = res - pow(2, numBits);
	}
	return res;
}

double ExtractField(unsigned char dataBytes[], int startByte, int endByte, int bitsOfFirstByte, int bitsOfLastByte)
{
	int msByteShift;
	int lsByteShift;
	unsigned int temp;
	double res;

	/* number of bits of dataBits[startByte] to shift */
	msByteShift = 8 - bitsOfFirstByte;
	/* number of bits of dataBits[endByte] to shift */
	lsByteShift = 8 - bitsOfLastByte;

	/* the bits of the field to extract are on 1 dataBits element */
    if(startByte==endByte)
    {
    	/* take the less-significant bitsOfLastByte bits */
    	if(msByteShift == 8)
    	{
    		//res = dataBits[startByte] >> lsByteShift;
    		temp = dataBytes[startByte];
			temp = (temp << (24 + lsByteShift)) >> (24 + lsByteShift);
			res = temp;
    	}
    	/* take the most-significant bitsOfFirstByte bits */
    	else
    	{
    		temp = (unsigned int)(dataBytes[startByte] >> msByteShift);
    		if(lsByteShift<8)
    		{
    			temp = (temp << (24 + lsByteShift + msByteShift)) >> (24 + lsByteShift + msByteShift);
    		}
    		res = temp;
    	}
    }
    /* the bits of the field to extract are on several (endByte-startByte+1) consecutive dataBits element */
    else
    {
		temp = dataBytes[startByte];
		/* take the bitsOfFirstByte less-significant bits of the dataBits[startByte] and copy in a integer of 32 bit variable (temp) */
		if(msByteShift != 0)
		{
			temp = (temp << (24 + msByteShift)) >> (24 + msByteShift);
		}
		/* left shift to give space to the consecutive bytes */
		temp = ( temp << ((8 * (endByte-startByte))-lsByteShift) );
		/* assignment to the final result */
		res = temp;
		/*
		 * if present, take the full bytes in the middle (between dataBits[startByte] and dataBits[endByte]) and put them in the right
		 * position inside the 32 bit variable (temp)
		 */
		int i;
		for(i = startByte+1; i < endByte; i++)
		{
			temp = dataBytes[i];
			temp = ( temp << ((8 * (endByte-i))-lsByteShift) );
			/* sum to the final result */
			res += temp;
		}
		/* take the bitsOfLastByte most-significant bit inside dataBits[endByte] */
		temp = dataBytes[endByte];
		temp = temp >> lsByteShift;
		/* sum to the final result */
		res += temp;
    }
	return res;
}


void ExtractBits(int startByte, int endByte, int bitsOfFirstByte, int bitsOfLastByte, unsigned char inputBytes[], unsigned char outputBytes[])
{
	int inIdx;
	int outIdx;
	int numOfBits;
	unsigned char newByte;

	if((startByte<0) || (endByte<0) || (startByte>endByte) || (bitsOfFirstByte<0) || (bitsOfFirstByte>8) || (bitsOfLastByte<0) || (bitsOfLastByte>8))
	{
		/* wrong parameters. Do nothing! */
		return;
	}

	if(startByte==endByte)
	{
		/* if the start byte and the end byte are the same!
		 * Look the comment in the header for the meaning of the indexes*/
		if((bitsOfFirstByte==0) && (bitsOfLastByte>0))
		{
			/* The selected bits are the LS (case C) */
			newByte = inputBytes[startByte] << (8-bitsOfLastByte);
		}
		else if((bitsOfFirstByte>0) && (bitsOfLastByte==0))
		{
			/* The selected bits are the MS (case D) */
			newByte = inputBytes[startByte] & ((unsigned char)0xff<<(8-bitsOfFirstByte));
		}
		else if((bitsOfFirstByte>0) && (bitsOfLastByte>0) && ((bitsOfFirstByte+bitsOfLastByte)>8))
		{
			/* The selected bits are the in the middle (case E) */
			numOfBits = bitsOfFirstByte+bitsOfLastByte-8; /* number of selected bits */
			newByte = inputBytes[startByte] >> (8-bitsOfFirstByte); /* clean low bits */
			newByte = newByte << (8-numOfBits); /* move to MS part of the byte */
		}
		else /* if((bitsOfFirstByte==0) && (bitsOfLastByte==0)) */
		{
			/* no bits selected? */
			newByte = 0x00;
		}
		outputBytes[0] = newByte;
		return;
	}
	/* select from multiple bytes */
	inIdx = startByte;
	outIdx = 0;
	numOfBits = 8 - bitsOfFirstByte; /* to be taken from startByte+1 to complete the first output byte */
	/* manage the case when the complete first byte is selected */
	if(bitsOfFirstByte==8)
	{
		while(inIdx<endByte)
		{
			outputBytes[outIdx] = inputBytes[inIdx];
			inIdx++;
			outIdx++;
		}
		/* last byte */
		outputBytes[outIdx] = inputBytes[endByte] & ((unsigned char)0xff<<(8-bitsOfLastByte));
	}
	else
	{
		/* the first byte is not completely selected */
		while((inIdx+1)<endByte)
		{
			newByte = (inputBytes[inIdx]<<numOfBits) | (inputBytes[inIdx+1]>>bitsOfFirstByte);
			outputBytes[outIdx] = newByte;
			inIdx++;
			outIdx++;
		}
		/* last bits */
		if((bitsOfFirstByte+bitsOfLastByte)==8)
		{
			newByte = (inputBytes[inIdx]<<numOfBits) | (inputBytes[inIdx+1]>>bitsOfFirstByte);
			outputBytes[outIdx] = newByte;
		}
		else if((bitsOfFirstByte+bitsOfLastByte)>8)
		{
			newByte = (inputBytes[inIdx]<<numOfBits) | (inputBytes[inIdx+1]>>bitsOfFirstByte);
			outputBytes[outIdx] = newByte;
			inIdx++;
			outIdx++;
			/* finish bits in the last byte */
			newByte = inputBytes[inIdx] >> (8-bitsOfLastByte); /* clean low bits */
			newByte = newByte << (8-bitsOfLastByte+numOfBits); /* move to MS part of the bytes */
			outputBytes[outIdx] = newByte;
		}
		else /* if((bitsOfFirstByte+bitsOfLastByte)<8) */
		{
			newByte = inputBytes[inIdx+1]>>(8-bitsOfLastByte);
			newByte = (inputBytes[inIdx]<<numOfBits) | (newByte<<(8-bitsOfLastByte-bitsOfFirstByte));
			outputBytes[outIdx] = newByte;
		}
	}
}


void CopyBitsUnaligned(int byteIdxIn,int bitIdxIn,unsigned char *arrayIn,int *byteIdxOutPtr,int *bitIdxOutPtr,unsigned char *arrayOut,int len)
{
	int byteIdxOut = *byteIdxOutPtr;
	unsigned char maskSetIn;
	unsigned char maskSetOut;
	unsigned char maskClearOut;
	int idx;

	if((bitIdxIn<0) || (bitIdxIn>7) || ((*bitIdxOutPtr)<0) || ((*bitIdxOutPtr)>7))
	{
		/* invalid parameters */
		return;
	}

	maskSetIn = ((unsigned char)0x01) << (7-bitIdxIn);
	maskSetOut = ((unsigned char)0x01) << (7-(*bitIdxOutPtr));
	maskClearOut = ~maskSetOut;

	for(idx=0;idx<len;idx++)
	{
		/* copy a bit */
		arrayOut[byteIdxOut] &= maskClearOut;
		if((arrayIn[byteIdxIn] & maskSetIn) != 0x00)
		{
			arrayOut[byteIdxOut] |= maskSetOut;
		}
		/* update bit indexes and (if needed) byte indexes */
		// input
		maskSetIn = maskSetIn>>1;
		if(maskSetIn==0x00)
		{
			maskSetIn = 0x80;
			byteIdxIn++;
		}
		// output
		(*bitIdxOutPtr)++;
		maskSetOut = maskSetOut>>1;
		if(maskSetOut==0x00)
		{
			(*bitIdxOutPtr) = 0;
			maskSetOut = 0x80;
			byteIdxOut++;
		}
		maskClearOut = ~maskSetOut;
	}
	*byteIdxOutPtr = byteIdxOut;
}
