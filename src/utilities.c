/*
 *	Copyright (c) 2023-2026, Signaloid.
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

#include <errno.h>
#include <math.h>
#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <uxhw.h>
#include <assert.h>
#include "utilities.h"
#include "kernel.h"
#include "common.h"

static const char *         kDefaultEEDataPath  = "EEPROM-calibration-data.csv";
static const char *         kDefaultRawDataPath = "raw-frame-data.csv";
static const unsigned int   kDefaultPixel       = (kMLX90640ConstantFrameBufferSize / 2) + (kMLX90640ConstantFrameWidth / 2);

void
printUsage(void)
{
	fprintf(stderr, "Example: MLX90640 sensor conversion routines - Signaloid version\n");
	fprintf(stderr, "\n");
	printCommonUsage();
	fprintf(
		stderr,
		"	[-c, --ee-data <path to sensor ee constants file: str (Default: '%s')>]\n"
		"	[-e, --emissivity <emissivity : float (Default: 'UniformDist(0.93, 0.97)')>]\n"
		"	[-q, --quantization-error] (Disable ADC quantization error.)\n"
		"	[-p, --pixel <Selected pixel : int, range = [0,%d] (Default: '%u')>]\n"
		"	[-a, --print-all-temperatures] (Print all temperature measurements.)\n",
		kDefaultEEDataPath,
		kMLX90640ConstantFrameBufferSize - 1,
		kDefaultPixel
	);
	fprintf(stderr, "\n");
}

void
setDefaultCommandLineArguments(CommandLineArguments * arguments)
{
	assert(arguments != NULL);

/*
 *	Older GCC versions have a bug which gives a spurious warning for the C universal zero
 *	initializer `{0}`. Any workaround makes the code less portable or prevents the common code
 *	from adding new fields to the `CommonCommandLineArguments` struct. Therefore, we surpress
 *	this warning.
 *
 *	See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53119.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-braces"

	*arguments = (CommandLineArguments) {
		.common                 = (CommonCommandLineArguments) { 0 },
		.eeDataPath             = "",
		.rawDataPath            = "",
		.modelQuantizationError = true,
		.printAllTemperatures   = false,
		.emissivity             = UxHwFloatUniformDist(kMLX90640ConstantEmissivityDistributionLowerBound, kMLX90640ConstantEmissivityDistributionUpperBound),
		.pixel = kDefaultPixel,
	};
#pragma GCC diagnostic pop

	snprintf(
		arguments->eeDataPath,
		kCommonConstantMaxCharsPerFilepath,
		"%s",
		(char *) kDefaultEEDataPath
	);
	snprintf(
		arguments->rawDataPath,
		kCommonConstantMaxCharsPerFilepath,
		"%s",
		(char *) kDefaultRawDataPath
	);
}

CommonConstantReturnType
getCommandLineArguments(int argc, char *  argv[], CommandLineArguments *  arguments)
{
	const char *    eeDataArg                   = NULL;
	const char *    emissivityArg               = NULL;
	const char *    pixelArg                    = NULL;
	bool            disableQuantisationError    = false;

	assert(arguments != NULL);
	setDefaultCommandLineArguments(arguments);

	DemoOption options[] = {
		{ .opt = "c", .optAlternative = "ee-data",                .hasArg = true,  .foundArg = &eeDataArg,     .foundOpt = NULL                             },
		{ .opt = "e", .optAlternative = "emissivity",             .hasArg = true,  .foundArg = &emissivityArg, .foundOpt = NULL                             },
		{ .opt = "q", .optAlternative = "quantization-error",     .hasArg = false, .foundArg = NULL,           .foundOpt = &disableQuantisationError        },
		{ .opt = "p", .optAlternative = "pixel",                  .hasArg = true,  .foundArg = &pixelArg,      .foundOpt = NULL                             },
		{ .opt = "a", .optAlternative = "print-all-temperatures", .hasArg = false, .foundArg = NULL,           .foundOpt = &arguments->printAllTemperatures },
		{ 0 },
	};

	if (parseArgs(argc, argv, &arguments->common, options) != 0)
	{
		fprintf(stderr, "Parsing command line arguments failed\n");
		printUsage();

		return kCommonConstantReturnTypeError;
	}

	arguments->modelQuantizationError = !disableQuantisationError;

	if (arguments->common.isHelpEnabled)
	{
		printUsage();
		exit(EXIT_SUCCESS);
	}

	if ((strcmp(arguments->common.outputFilePath, "") != 0) || arguments->common.isWriteToFileEnabled)
	{
		fprintf(stderr, "Error: This application does not support saving outputs to file.\n");
		exit(EXIT_FAILURE);
	}

	/*
	 *	If no output selected from CLA, set the print all value as default.
	 */
	if (!arguments->common.isOutputSelected)
	{
		arguments->common.outputSelect = kOutputVariableIndexMax;
	}

	/*
	 *	Validate output select range.
	 */
	if (arguments->common.outputSelect > kOutputVariableIndexMax)
	{
		fprintf(
			stderr,
			"Error: Output select value (-S option) is greater than the possible number of outputs: Provided %zu. Max: %d\n",
			arguments->common.outputSelect,
			kOutputVariableIndexMax
		);
		exit(EXIT_FAILURE);
	}
	/*
	 *	When all outputs are selected, we cannot be in benchmarking mode or Monte Carlo mode.
	 */
	else if (arguments->common.outputSelect == kOutputVariableIndexMax)
	{
		if ((arguments->common.isBenchmarkingMode) || (arguments->common.isMonteCarloMode))
		{
			fprintf(stderr, "Error: Please select a single output when in benchmarking mode or Monte Carlo mode.\n");
			exit(EXIT_FAILURE);
		}
	}

	if (arguments->common.isVerbose)
	{
		fprintf(stderr, "Error: Verbose mode not supported.\n");
		exit(EXIT_FAILURE);
	}


	if (eeDataArg != NULL)
	{
		int ret = snprintf(arguments->eeDataPath, kCommonConstantMaxCharsPerFilepath, "%s", eeDataArg);

		if ((ret < 0) || (ret >= kCommonConstantMaxCharsPerFilepath))
		{
			fprintf(stderr, "Error: Could not read ee data file path from command line arguments.\n");
			printUsage();

			return kCommonConstantReturnTypeError;
		}
	}

	if (emissivityArg != NULL)
	{
		float   emissivity;
		int     ret = parseFloatChecked(emissivityArg, &emissivity);

		if (ret != kCommonConstantReturnTypeSuccess)
		{
			fprintf(stderr, "Error: The emissivity must be a real number.\n");
			printUsage();

			return kCommonConstantReturnTypeError;
		}

		arguments->emissivity = emissivity;
	}

	if (pixelArg != NULL)
	{
		int pixel;
		int ret = parseIntChecked(pixelArg, &pixel);

		if (ret != kCommonConstantReturnTypeSuccess)
		{
			fprintf(stderr, "Error: The pixel must be an integer.\n");
			printUsage();

			return kCommonConstantReturnTypeError;
		}

		if (pixel < 0)
		{
			fprintf(stderr, "Error: The pixel must be non-negative.\n");
			printUsage();

			return kCommonConstantReturnTypeError;
		}

		if (pixel >= kMLX90640ConstantFrameBufferSize)
		{
			fprintf(stderr, "Error: The pixel must be less than %d.\n", kMLX90640ConstantFrameBufferSize);
			printUsage();

			return kCommonConstantReturnTypeError;
		}

		arguments->pixel = pixel;
	}

	if (strcmp(arguments->common.inputFilePath, "") != 0)
	{
		strcpy(arguments->rawDataPath, arguments->common.inputFilePath);
	}

	return kCommonConstantReturnTypeSuccess;
}


int
readUint16DataFromCSV(uint16_t * dest, int line, int maxLen, const char *  filename)
{
	/*
	 *	Open the CSV file for reading
	 */
	FILE * file;
	/*
	 *	Assuming each line in the CSV file is no longer than 1024*1024 characters
	 */
	char    lineBuffer[kCommonConstantMaxCharsPerLine];
	int     currentLine = 0;

	file = fopen(filename, "r");

	if (file == NULL)
	{
		fprintf(stderr, "Error: Failed to open CSV file '%s': %s\n", filename, strerror(errno));
		exit(EXIT_FAILURE);
	}

	while (fgets(lineBuffer, sizeof(lineBuffer), file))
	{
		if (currentLine == line)
		{
			char *  token   = strtok(lineBuffer, ",");
			int     index   = 0;

			while ((token != NULL) && (index < maxLen))
			{
				dest[index] = (uint16_t) strtoul(token, NULL, 10);
				token       = strtok(NULL, ",");
				index++;
			}

			fclose(file);

			/*
			 *	Return the number of uint16_t values read
			 */
			return index;
		}

		currentLine++;
	}

	fclose(file);

	/*
	 *	Line not found.
	 */
	return -1;
}
