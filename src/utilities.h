/*
 *	Copyright (c) 2023, Signaloid.
 *
 *	Permission is hereby granted, free of charge, to any person obtaining a copy
 *	of this software and associated documentation files (the "Software"), to deal
 *	in the Software without restriction, including without limitation the rights
 *	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *	copies of the Software, and to permit persons to whom the Software is
 *	furnished to do so, subject to the following conditions:
 *
 *	The above copyright notice and this permission notice shall be included in all
 *	copies or substantial portions of the Software.
 *
 *	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *	SOFTWARE.
 */

#pragma once

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

#include "common.h"

typedef enum
{
	kMLX90640ConstantEEDataBufferSize 	= 832,
	kMLX90640ConstantRawFrameBufferSize	= 834,
	kMLX90640ConstantFrameBufferSize	= 768, /* 32*24 */
	kMLX90640ConstantFrameWidth		= 32,
	kMLX90640ConstantFrameHeight		= 24,
	kMLX90640ConstantTaShift		= 8,
} MLX90640Constant;

typedef struct CommandLineArguments
{
	CommonCommandLineArguments	common;

	char				eeDataPath[kCommonConstantMaxCharsPerFilepath];
	char				rawDataPath[kCommonConstantMaxCharsPerFilepath];
	bool				modelQuantizationError;
	bool				printAllTemperatures;
	float				emissivity;
	unsigned int			pixel;
} CommandLineArguments;

/**
 *	@brief	Print out command line usage.
 */
void	printUsage(void);

/**
 *	@brief	Get command line arguments.
 *
 *	@param	argc		: argument count from main()
 *	@param	argv		: argument vector from main()
 *	@param	arguments	: Pointer to struct to store arguments
 *	@return			: `kCommonConstantSuccess` if successful, else `kCommonConstantError`
 */
CommonConstantReturnType getCommandLineArguments(int argc, char *  argv[], CommandLineArguments *  arguments);


/**
 *	@brief	Read raw uint16 adc data from file. Like read(2), returns number of elements read or -1 on failure.
 *
 *	@param	dest			: destination of data
 *	@param	line			: line of CSV file with raw data
 *	@param	maxLen			: maximum length of uint16 values in csv line
 *	@param	filename		: raw data csv file path
 *	@return	int			: number of values read if successful, else -1
 */
int	readUint16DataFromCSV(uint16_t *  dest, int line, int maxLen, const char *  filename);

#define kMLX90640ConstantEmissivityDistributionLowerBound	(0.93)
#define kMLX90640ConstantEmissivityDistributionUpperBound	(0.97)