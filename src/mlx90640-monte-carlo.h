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

#include "utilities.h"

/**
 *	@brief	Monte Carlo calculation kernel. Runs
 *		`arguments->common.numberOfMonteCarloIterations` independent
 *		evaluations of the MLX90640 conversion routine into
 *		`monteCarloOutputSamples`. Each iteration resamples
 *		`arguments->emissivity` via `mlx90640SetEmissivityViaUxHwCall`
 *		(in `kernel.c`) and then reads and calibrates one full raw frame
 *		via `mlx90640CalculatePixelTemperature` (also in `kernel.c`),
 *		identically to the original per-iteration loop.
 *
 *		This is not a UxHw-free path: the per-iteration emissivity
 *		resampling is itself a UxHw distributional call
 *		(`UxHwFloatUniformDist`), and the raw-frame ADC quantization
 *		noise is likewise modelled via `UxHwFloatUniformDist` inside
 *		`MLX90640_CalculateTo_UT` (reached through
 *		`mlx90640CalculatePixelTemperature` -> `calculateOutput`).
 *		Re-deriving either without UxHw calls would be a semantics
 *		change, not a restructure. Both UxHw calls live in `kernel.c`,
 *		so this file itself contains no UxHw distributional API calls --
 *		the same split used by the FLIR Ax5 sibling demo's
 *		`FLIRAx5MonteCarlo`.
 *
 *		Writes the last iteration's calibrated pixel temperature to
 *		`outputVariables[kOutputVariableIndexFirstOutput]`.
 *
 *	@param	arguments		: Command-line arguments.
 *	@param	outputVariables		: Array of size `kOutputVariableIndexMax` to fill.
 *	@param	monteCarloOutputSamples	: Array of `numberOfMonteCarloIterations` doubles, filled with samples.
 *	@return	double			: Returns the calibrated pixel temperature of the last iteration.
 */
double
mlx90640MonteCarlo(
	CommandLineArguments *  arguments,
	double *                outputVariables,
	double *                monteCarloOutputSamples);
