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

#pragma once

#include "MLX90640_API.h"
#include "common.h"
#include "utilities.h"

#define kMLX90640ConstantEmissivityDistributionLowerBound   (0.93)
#define kMLX90640ConstantEmissivityDistributionUpperBound   (0.97)

typedef enum
{
	kOutputVariableIndexFirstOutput = 0,
	kOutputVariableIndexMax,
} OutputVariableIndex;

/**
 *	@brief	Convert a raw data frame to an array of temperatures.
 *
 *	@param	mlx90640Params	: Parameters of MLX90640 sensor.
 *	@param	line		: Line in raw data CSV file to parse. Each line contains one raw data frame.
 *	@param	arguments	: Pointer to command line arguments struct.
 *	@return	int		: Size of raw data frame that was converted if successful, else -1.
 */
int
calculateOutput(paramsMLX90640 * mlx90640Params, size_t line, CommandLineArguments * arguments);

/**
 *	@brief	Read a full raw frame (all sub-pages) via `calculateOutput` and
 *		return the calibrated temperature of `arguments->pixel`.
 *
 *	@param	arguments	: Command-line arguments. `arguments->rawDataPath`
 *				  selects the input CSV and `arguments->pixel`
 *				  selects the returned pixel.
 *	@return	double		: Returns the calibrated temperature of `arguments->pixel`.
 */
double
mlx90640CalculatePixelTemperature(CommandLineArguments * arguments);

/**
 *	@brief	Resample `arguments->emissivity` from
 *		`UxHwDoubleUniformDist(kMLX90640ConstantEmissivityDistributionLowerBound,
 *		kMLX90640ConstantEmissivityDistributionUpperBound)` (via
 *		`UxHwFloatUniformDist`). Used only by the Monte Carlo-mode kernel
 *		(`mlx90640MonteCarlo`, in `mlx90640-monte-carlo.c`), once per
 *		iteration, matching the original per-iteration resampling loop.
 *
 *	@param	arguments	: Command-line arguments; `arguments->emissivity` is overwritten.
 */
void
mlx90640SetEmissivityViaUxHwCall(CommandLineArguments * arguments);

/**
 *	@brief	UxHw-mode calculation kernel. Computes the (single) selected
 *		output using a single distributional evaluation of the MLX90640
 *		conversion routine. Writes the result into `outputVariables` and
 *		`monteCarloOutputSamples[0]`.
 *
 *	@param	arguments		: Command-line arguments.
 *	@param	outputVariables		: Array of size `kOutputVariableIndexMax` to fill.
 *	@param	monteCarloOutputSamples	: Single-element array for the distributional result.
 *	@return	double			: Returns the calibrated pixel temperature.
 */
double
calculateOutputUxHw(
	CommandLineArguments *  arguments,
	double *                outputVariables,
	double *                monteCarloOutputSamples);

/**
 *	@brief	Monte Carlo calculation kernel. Runs
 *		`arguments->common.numberOfMonteCarloIterations` independent
 *		evaluations into `monteCarloOutputSamples`, resampling
 *		`arguments->emissivity` on every iteration, and computes the
 *		(single) selected output. Writes the result into
 *		`outputVariables`.
 *
 *	@param	arguments		: Command-line arguments.
 *	@param	outputVariables		: Array of size `kOutputVariableIndexMax` to fill.
 *	@param	monteCarloOutputSamples	: Array of `numberOfMonteCarloIterations` doubles, filled with samples.
 *	@return	double			: Returns the calibrated pixel temperature of the last iteration.
 */
double
calculateOutputMonteCarlo(
	CommandLineArguments *  arguments,
	double *                outputVariables,
	double *                monteCarloOutputSamples);
