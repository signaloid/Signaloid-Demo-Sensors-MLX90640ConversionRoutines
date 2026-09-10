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

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <uxhw.h>
#include "MLX90640_API.h"
#include "common.h"
#include "utilities.h"
#include "kernel.h"
#include "mlx90640-uxhw.h"
#include "mlx90640-monte-carlo.h"

extern uint16_t         rawDataFrame[];
extern float            mlx90640To[];
extern paramsMLX90640   mlx90640Params;

/**
 *	@brief	Calculate calibrated temperatures frame. Modified from Melexis original library to model ADC quantization error.
 *
 *	@param	frameData		: Raw data frame from MLX90640.
 *	@param	params			: Parameters of MLX90640 sensor.
 *	@param	emissivity		: Emissivity of the measured object.
 *	@param	tr			: Reflected temperature based on the sensor ambient temperature.
 *	@param	result			: Pointer to float array for storing calibrated temperatures.
 *	@param	quantizationError	: Enable modeling of ADC quantization error.
 */
static void
MLX90640_CalculateTo_UT(
	uint16_t *              frameData,
	const paramsMLX90640 *  params,
	float                   emissivity,
	float                   tr,
	float *                 result,
	bool                    quantizationError)
{
	float       vdd;
	float       ta;
	float       ta4;
	float       tr4;
	float       taTr;
	float       gain;
	float       irDataCP[2];
	float       irData;
	int16_t     tempInt;
	float       alphaCompensated;
	uint8_t     mode;
	int8_t      ilPattern;
	int8_t      chessPattern;
	int8_t      pattern;
	int8_t      conversionPattern;
	float       Sx;
	float       To;
	float       alphaCorrR[4];
	int8_t      range;
	uint16_t    subPage;
	float       ktaScale;
	float       kvScale;
	float       alphaScale;
	float       kta;
	float       kv;

	subPage = frameData[833];
	vdd     = MLX90640_GetVdd(frameData, params);
	ta      = MLX90640_GetTa(frameData, params);

	ta4     = (ta + 273.15);
	ta4     = ta4 * ta4;
	ta4     = ta4 * ta4;
	tr4     = (tr + 273.15);
	tr4     = tr4 * tr4;
	tr4     = tr4 * tr4;
	taTr    = tr4 - (tr4 - ta4) / emissivity;

	ktaScale    = POW2(params->ktaScale);
	kvScale     = POW2(params->kvScale);
	alphaScale  = POW2(params->alphaScale);

	alphaCorrR[0]   = 1 / (1 + params->ksTo[0] * 40);
	alphaCorrR[1]   = 1;
	alphaCorrR[2]   = (1 + params->ksTo[1] * params->ct[2]);
	alphaCorrR[3]   = alphaCorrR[2] * (1 + params->ksTo[2] * (params->ct[3] - params->ct[2]));

	/*
	 *	------------------------- Gain calculation -----------------------------------
	 */

	gain = (float) params->gainEE / (int16_t) frameData[778];

	/*
	 *	------------------------- To calculation -------------------------------------
	 */
	mode = (frameData[832] & MLX90640_CTRL_MEAS_MODE_MASK) >> 5;

	irDataCP[0] = (int16_t) frameData[776] * gain;
	irDataCP[1] = (int16_t) frameData[808] * gain;

	irDataCP[0] = irDataCP[0] - params->cpOffset[0] * (1 + params->cpKta * (ta - 25)) *
	              (1 + params->cpKv * (vdd - 3.3));

	if (mode == params->calibrationModeEE)
	{
		irDataCP[1] = irDataCP[1] - params->cpOffset[1] * (1 + params->cpKta * (ta - 25)) *
		              (1 + params->cpKv * (vdd - 3.3));
	}
	else
	{
		irDataCP[1] = irDataCP[1] - (params->cpOffset[1] + params->ilChessC[0]) *
		              (1 + params->cpKta * (ta - 25)) *
		              (1 + params->cpKv * (vdd - 3.3));
	}

	for (int pixelNumber = 0; pixelNumber < 768; pixelNumber++)
	{
		ilPattern           = pixelNumber / 32 - (pixelNumber / 64) * 2;
		chessPattern        = ilPattern ^ (pixelNumber - (pixelNumber / 2) * 2);
		conversionPattern   = ((pixelNumber + 2) / 4 - (pixelNumber + 3) / 4 +
		                       (pixelNumber + 1) / 4 - pixelNumber / 4) *
		                      (1 - 2 * ilPattern);

		if (mode == 0)
		{
			pattern = ilPattern;
		}
		else
		{
			pattern = chessPattern;
		}

		if (pattern == frameData[833])
		{
			/*
			 *	Signaloid modification: model ADC quantization error using Uniform
			 *	Dist Original: irData = tempInt * gain;
			 */
			tempInt = (int16_t) frameData[pixelNumber];

			if (quantizationError)
			{
				irData = UxHwFloatUniformDist((float) tempInt - 0.5, (float) tempInt + 0.5) * gain;
			}
			else
			{
				irData = tempInt * gain;
			}

			kta     = params->kta[pixelNumber] / ktaScale;
			kv      = params->kv[pixelNumber] / kvScale;
			irData  = irData - params->offset[pixelNumber] * (1 + kta * (ta - 25)) * (1 + kv * (vdd - 3.3));

			if (mode != params->calibrationModeEE)
			{
				irData = irData + params->ilChessC[2] * (2 * ilPattern - 1) - params->ilChessC[1] * conversionPattern;
			}

			irData  = irData - params->tgc * irDataCP[subPage];
			irData  = irData / emissivity;

			alphaCompensated    = SCALEALPHA * alphaScale / params->alpha[pixelNumber];
			alphaCompensated    = alphaCompensated * (1 + params->KsTa * (ta - 25));

			Sx  = alphaCompensated * alphaCompensated * alphaCompensated * (irData + alphaCompensated * taTr);
			Sx  = sqrt(sqrt(Sx)) * params->ksTo[1];
			To  = sqrt(sqrt(irData / (alphaCompensated * (1 - params->ksTo[1] * 273.15) + Sx) + taTr)) - 273.15;

			if (To < params->ct[1])
			{
				range = 0;
			}
			else if (To < params->ct[2])
			{
				range = 1;
			}
			else if (To < params->ct[3])
			{
				range = 2;
			}
			else
			{
				range = 3;
			}

			To = sqrt(sqrt(irData / (alphaCompensated * alphaCorrR[range] * (1 + params->ksTo[range] * (To - params->ct[range]))) + taTr)) - 273.15;
			result[pixelNumber] = To;
		}
	}
}

int
calculateOutput(paramsMLX90640 * mlx90640Params, size_t line, CommandLineArguments * arguments)
{
	int     ret;
	float   tr;

	ret = readUint16DataFromCSV(
		rawDataFrame,
		line,
		kMLX90640ConstantRawFrameBufferSize,
		arguments->rawDataPath
	);

	if (ret <= 0)
	{
		return -1;
	}

	tr = MLX90640_GetTa(rawDataFrame, mlx90640Params) - kMLX90640ConstantTaShift;
	MLX90640_CalculateTo_UT(
		rawDataFrame,
		mlx90640Params,
		arguments->emissivity,
		tr,
		mlx90640To,
		arguments->modelQuantizationError
	);

	return ret;
}

double
mlx90640CalculatePixelTemperature(CommandLineArguments * arguments)
{
	size_t ii = 0;

	/*
	 *	Conversion routines need to process at least 2 sub-pages.
	 */
	while (calculateOutput(&mlx90640Params, ii, arguments) != -1)
	{
		ii++;
	}

	if (ii < 2)
	{
		fprintf(stderr, "Error in reading sensor raw data %s \n", arguments->rawDataPath);
		exit(EXIT_FAILURE);
	}

	return (double) mlx90640To[arguments->pixel];
}

void
mlx90640SetEmissivityViaUxHwCall(CommandLineArguments * arguments)
{
	arguments->emissivity = UxHwFloatUniformDist(
		kMLX90640ConstantEmissivityDistributionLowerBound,
		kMLX90640ConstantEmissivityDistributionUpperBound
	);

	return;
}

double
calculateOutputUxHw(
	CommandLineArguments *  arguments,
	double *                outputVariables,
	double *                monteCarloOutputSamples)
{
	return mlx90640UxHw(arguments, outputVariables, monteCarloOutputSamples);
}

double
calculateOutputMonteCarlo(
	CommandLineArguments *  arguments,
	double *                outputVariables,
	double *                monteCarloOutputSamples)
{
	return mlx90640MonteCarlo(arguments, outputVariables, monteCarloOutputSamples);
}
