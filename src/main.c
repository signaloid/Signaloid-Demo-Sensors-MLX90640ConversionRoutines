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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <uxhw.h>
#include <time.h>
#include "MLX90640_API.h"
#include "utilities.h"
#include "common.h"
#include "kernel.h"

#ifdef NO_OS_AVAILABLE

void
returnZeroNoOS(void);
#endif

static uint16_t eeData[kMLX90640ConstantEEDataBufferSize];
uint16_t        rawDataFrame[kMLX90640ConstantRawFrameBufferSize];
float           mlx90640To[kMLX90640ConstantFrameBufferSize];
paramsMLX90640  mlx90640Params = { 0 };

int
main(int argc, char *  argv[])
{
	CommandLineArguments        arguments;
	double                      output;
	double *                    monteCarloOutputSamples = NULL;
	clock_t                     start               = 0;
	clock_t                     end                 = 0;
	double                      cpuTimeInSeconds    = 0.0;
	double                      outputVariables[kOutputVariableIndexMax];
	const char *                applicationDescription = "MLX90640 Sensor Conversion Routines - Signaloid Version";
	const char *                outputVariableNames[kOutputVariableIndexMax] = {
		[kOutputVariableIndexFirstOutput] = "Pixel 400",
	};
	const char *                outputVariableDescriptions[kOutputVariableIndexMax] = {
		[kOutputVariableIndexFirstOutput] = "Temperature of Pixel 400",
	};
	kOutputVariableTypeIndex    outputVariableTypes[kOutputVariableIndexMax] = {
		[kOutputVariableIndexFirstOutput] = kOutputVariableTypeDistribution,
	};
	MeanAndVariance             meanAndVariance;

	/*
	 *	Get command-line arguments.
	 */
	if (getCommandLineArguments(argc, argv, &arguments))
	{
		return kCommonConstantReturnTypeError;
	}

	/*
	 *	Load ee data from sensor.
	 */
	if (readUint16DataFromCSV(eeData, 0, kMLX90640ConstantEEDataBufferSize, arguments.eeDataPath) < kMLX90640ConstantEEDataBufferSize)
	{
		fprintf(stderr, "Error in reading sensor ee data\n");
		exit(EXIT_FAILURE);
	}

	/*
	 *	MonteCarlo output samples are used even in the UxHw use case to store
	 *	the result of the single distributional evaluation.
	 */
	monteCarloOutputSamples =
		(double *) checkedMalloc(
			arguments.common.numberOfMonteCarloIterations * sizeof(double),
			__FILE__,
			__LINE__
		);

	/*
	 *	Start timing.
	 */
	if (arguments.common.isTimingEnabled)
	{
		start = clock();
	}

	/*
	 *	Extract parameters.
	 */
	if (MLX90640_ExtractParameters(eeData, &mlx90640Params))
	{
		fprintf(stderr, "Error in extracting parameters from EE\n");
		exit(EXIT_FAILURE);
	}

	/*
	 *	Dispatch to the mode-specific kernel. The Monte Carlo loop lives
	 *	inside `calculateOutputMonteCarlo`; UxHw mode runs a single
	 *	distributional evaluation inside `calculateOutputUxHw`.
	 */
	bool isSelectedOutputScalar = (arguments.common.outputSelect != kOutputVariableIndexMax) &&
	                              (outputVariableTypes[arguments.common.outputSelect] == kOutputVariableTypeScalar);

	if (arguments.common.isMonteCarloMode)
	{
		output = calculateOutputMonteCarlo(&arguments, outputVariables, monteCarloOutputSamples);

		/*
		 *	If not doing UxHw version, then approximate the cost of the third phase of
		 *	Monte Carlo (post-processing), by calculating the mean and variance.
		 */
		if (!isSelectedOutputScalar)
		{
			meanAndVariance = calculateMeanAndVarianceOfDoubleSamples(monteCarloOutputSamples, arguments.common.numberOfMonteCarloIterations);
			output          = outputVariables[arguments.common.outputSelect] = meanAndVariance.mean;
		}
	}
	else
	{
		output = calculateOutputUxHw(&arguments, outputVariables, monteCarloOutputSamples);
	}

	/*
	 *	Stop timing.
	 */
	if (arguments.common.isTimingEnabled)
	{
		end                 = clock();
		cpuTimeInSeconds    = ((double) (end - start)) / CLOCKS_PER_SEC;
	}

	/*
	 *	For scalar outputs in Monte Carlo mode, present a copy of the args with MC
	 *	disabled and iterations=1 so the common print routines take their scalar
	 *	code paths instead of computing distribution stats over a one-element buffer.
	 *	This demo's single output is always a distribution, so `printArguments` is
	 *	always identical to `arguments.common`; kept for structural consistency.
	 */
	CommonCommandLineArguments printArguments = arguments.common;

	if (arguments.common.isMonteCarloMode && isSelectedOutputScalar)
	{
		printArguments.isMonteCarloMode             = false;
		printArguments.numberOfMonteCarloIterations = 1;
	}

	/*
	 *	Print the results (either in JSON or standard output format).
	 */
	if (arguments.common.isOutputJSONMode)
	{
		printJSONFormattedOutput(
			&printArguments,
			monteCarloOutputSamples,
			outputVariables,
			outputVariableNames,
			kOutputVariableIndexMax,
			applicationDescription
		);
	}
	else
	{
		printHumanConsumableOutput(
			&printArguments,
			kOutputVariableIndexMax,
			outputVariables,
			outputVariableNames,
			outputVariableDescriptions,
			monteCarloOutputSamples
		);
	}

	/*
	 *	Print timing result.
	 */
	if (arguments.common.isTimingEnabled)
	{
		printf("\nCPU time used: %" SignaloidParticleModifier "lf seconds\n", cpuTimeInSeconds);
	}

	/*
	 *	Write output data. `getCommandLineArguments()` exits the process
	 *	earlier if `isWriteToFileEnabled` is set (this application does not
	 *	support saving outputs to file), so this is unreachable; kept for
	 *	structural consistency with the other Signaloid demos.
	 */
	if (arguments.common.isWriteToFileEnabled)
	{
		if (writeOutputDoubleDistributionsToCSV(
				arguments.common.outputFilePath,
				outputVariables,
				outputVariableNames,
				kOutputVariableIndexMax
		))
		{
			return kCommonConstantReturnTypeError;
		}
	}

	/*
	 *	Save Monte carlo outputs in an output file.
	 *	Free dynamically-allocated memory.
	 */
	if (arguments.common.isMonteCarloMode)
	{
		size_t samplesToSave = isSelectedOutputScalar
		                ? 1
		                : arguments.common.numberOfMonteCarloIterations;

		saveMonteCarloDoubleDataToDataDotOutFile(
			monteCarloOutputSamples,
			(uint64_t) (cpuTimeInSeconds * 1000000),
			samplesToSave
		);
	}
	free(monteCarloOutputSamples);

#ifdef NO_OS_AVAILABLE
	returnZeroNoOS();
#else

	return 0;

#endif
}
