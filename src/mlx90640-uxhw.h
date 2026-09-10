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
 *	@brief	UxHw-mode calculation kernel. Performs a single distributional
 *		evaluation of the MLX90640 conversion routine: reads one full raw
 *		frame and calibrates it via `mlx90640CalculatePixelTemperature`
 *		(in `kernel.c`), using the emissivity already present in
 *		`arguments->emissivity`.
 *
 *		Writes the result to
 *		`outputVariables[kOutputVariableIndexFirstOutput]` and to
 *		`monteCarloOutputSamples[0]`.
 *
 *	@param	arguments		: Command-line arguments.
 *	@param	outputVariables		: Array of size `kOutputVariableIndexMax` to fill.
 *	@param	monteCarloOutputSamples	: Single-element array for the distributional result.
 *	@return	double			: Returns the calibrated pixel temperature.
 */
double
mlx90640UxHw(
	CommandLineArguments *  arguments,
	double *                outputVariables,
	double *                monteCarloOutputSamples);
