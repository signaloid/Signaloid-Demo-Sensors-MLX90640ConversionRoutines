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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <uxhw.h>
#include <MLX90640_API.h>
#include "utilities.h"
#include "common.h"

static uint16_t	eeData[kMLX90640ConstantEEDataBufferSize];
static uint16_t	rawDataFrame[kMLX90640ConstantRawFrameBufferSize];
static float	mlx90640To[kMLX90640ConstantFrameBufferSize];

/**
 *	@brief	Convert a data raw data frame to array of temperatures.
 *
 *	@param	mlx90640Params	: Parameters of MLX90640 sensor.
 *	@param	line		: Line in raw data CSV file to parse. Each line contains one raw data frame.
 *	@param	arguments	: Pointer to command line arguments struct.
 *	@return	int		: Size of raw data frame that was converted if successful, else -1.
 */
static int processDataFrame(paramsMLX90640 *  mlx90640Params, size_t line, CommandLineArguments *  arguments);

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
static void MLX90640_CalculateTo_UT(uint16_t *  frameData, const paramsMLX90640 *  params, float emissivity, float tr, float *  result, bool quantizationError);

int
main(int argc, char *  argv[])
{
	CommandLineArguments	arguments;
	paramsMLX90640		mlx90640Params = { 0 };
	float			pixelTemp = 0.0;
	clock_t			start = 0;
	clock_t			end = 0;
	double			cpuTimeUsed;

	/*
	 *	Get command line arguments.
	 */
	if (getCommandLineArguments(argc, argv, &arguments))
	{
		exit(EXIT_FAILURE);
	}

	/*
	 *	Load ee data from sensor
	 */
	if (readUint16DataFromCSV(eeData, 0, kMLX90640ConstantEEDataBufferSize, arguments.eeDataPath) < kMLX90640ConstantEEDataBufferSize)
	{
		fprintf(stderr, "Error in reading sensor ee data\n");
		exit(EXIT_FAILURE);
	}

	/*
	 *	Start timing.
	 */
	if (arguments.common.isTimingEnabled)
	{
		start = clock();
	}

	/*
	 *	Loop process kernel. This is used when benchmarking equivalent monte carlo
	 *	execution time. In all other cases i == 1; 
	 */
	for (size_t j = 0; j < arguments.common.numberOfMonteCarloIterations; ++j)
	{
		if (MLX90640_ExtractParameters(eeData, &mlx90640Params))
		{
			fprintf(stderr, "Error in extracting parameters from EE\n");
			exit(EXIT_FAILURE);
		}

		/*
		 *	Conversion routines need to process at least 2 sub-pages.
		 */
		for (size_t i = 0;; i++)
		{
			/*
			 *	processDataFrame returns -1 when line i does not contain a 
			 *	valid mlx90640 frame
			 */
			if (processDataFrame(&mlx90640Params, i, &arguments) == -1)
			{
				if (i < 1)
				{
					fprintf(stderr, "Error in reading sensor raw data\n");
					exit(EXIT_FAILURE);
				}
				break;
			}
		}

		doNotOptimize((void*)mlx90640To);

		pixelTemp = mlx90640To[arguments.pixel];
	}

	/*
	 *	Stop timing.
	 */
	if (arguments.common.isTimingEnabled)
	{
		end = clock();
		cpuTimeUsed = ((double)(end - start)) / CLOCKS_PER_SEC;
	}

	/*
	 *	Print outputs.
	 */
	if (!arguments.common.isOutputJSONMode)
	{
		printf("Converting raw data to temperature using emissivity = %f\n", arguments.emissivity);

		if (!arguments.printAllTemperatures)
		{
			printf("Temperature of pixel %u: %f Celsius.\n\n", arguments.pixel, pixelTemp);
		}
		else
		{
			for (size_t h = 0; h < kMLX90640ConstantFrameHeight; h++)
			{
				for (size_t w = 0; w < kMLX90640ConstantFrameWidth; w++)
				{
					printf("%f ", mlx90640To[h * kMLX90640ConstantFrameWidth + w]);
				}
				printf("\n");
			}
		}
	}

	/*
	 *	Print json outputs.
	 */
	if (arguments.common.isOutputJSONMode)
	{
		if (!arguments.printAllTemperatures)
		{
			JSONvariable variables[] = {
				{
					.variableSymbol = "temperature",
					.variableDescription = "Temperature (calibrated)",
					.values = (JSONvariablePointer) { .asFloat = &pixelTemp },
					.type = kJSONvariableTypeFloat,
					.size = 1,
				},
			};

			printJSONVariables(variables, 1, "MLX90640 Conversion Values.");
		}
		else
		{
			JSONvariable variables[] = {
				{
					.variableSymbol = "temperatures",
					.variableDescription = "Temperatures (calibrated)",
					.values = (JSONvariablePointer) { .asFloat = mlx90640To },
					.type = kJSONvariableTypeFloat,
					.size = kMLX90640ConstantFrameBufferSize,
				},
			};

			printJSONVariables(variables, 1, "MLX90640 Conversion Values.");
		}
	}

	/*
	 *	Print timing results.
	 */
	if ((arguments.common.isTimingEnabled) && (!arguments.common.isOutputJSONMode))
	{
		printf("CPU time used: %lf seconds\n", cpuTimeUsed);
	}

	return 0;
}

static int
processDataFrame(paramsMLX90640 *  mlx90640Params, size_t line, CommandLineArguments *  arguments)
{
	int	ret;
	float	tr;

	ret = readUint16DataFromCSV(
		rawDataFrame,
		line,
		kMLX90640ConstantRawFrameBufferSize,
		arguments->rawDataPath);

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
		arguments->modelQuantizationError);

	return ret;
}

static void
MLX90640_CalculateTo_UT(
	uint16_t *		frameData,
	const paramsMLX90640 *	params,
	float			emissivity,
	float			tr,
	float *			result,
	bool			quantizationError)
{
	float		vdd;
	float		ta;
	float		ta4;
	float		tr4;
	float		taTr;
	float		gain;
	float		irDataCP[2];
	float		irData;
	int16_t		tempInt;
	float		alphaCompensated;
	uint8_t		mode;
	int8_t		ilPattern;
	int8_t		chessPattern;
	int8_t		pattern;
	int8_t		conversionPattern;
	float		Sx;
	float		To;
	float		alphaCorrR[4];
	int8_t		range;
	uint16_t	subPage;
	float		ktaScale;
	float		kvScale;
	float		alphaScale;
	float		kta;
	float		kv;

	subPage = frameData[833];
	vdd = MLX90640_GetVdd(frameData, params);
	ta = MLX90640_GetTa(frameData, params);

	ta4 = (ta + 273.15);
	ta4 = ta4 * ta4;
	ta4 = ta4 * ta4;
	tr4 = (tr + 273.15);
	tr4 = tr4 * tr4;
	tr4 = tr4 * tr4;
	taTr = tr4 - (tr4 - ta4) / emissivity;

	ktaScale = POW2(params->ktaScale);
	kvScale = POW2(params->kvScale);
	alphaScale = POW2(params->alphaScale);

	alphaCorrR[0] = 1 / (1 + params->ksTo[0] * 40);
	alphaCorrR[1] = 1;
	alphaCorrR[2] = (1 + params->ksTo[1] * params->ct[2]);
	alphaCorrR[3] = alphaCorrR[2] * (1 + params->ksTo[2] * (params->ct[3] - params->ct[2]));

	/*
	 *	------------------------- Gain calculation -----------------------------------
	 */

	gain = (float)params->gainEE / (int16_t)frameData[778];

	/*
	 *	------------------------- To calculation -------------------------------------
	 */
	mode = (frameData[832] & MLX90640_CTRL_MEAS_MODE_MASK) >> 5;

	irDataCP[0] = (int16_t)frameData[776] * gain;
	irDataCP[1] = (int16_t)frameData[808] * gain;

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
		ilPattern = pixelNumber / 32 - (pixelNumber / 64) * 2;
		chessPattern = ilPattern ^ (pixelNumber - (pixelNumber / 2) * 2);
		conversionPattern = ((pixelNumber + 2) / 4 - (pixelNumber + 3) / 4 +
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
			tempInt = (int16_t)frameData[pixelNumber];
			if (quantizationError)
			{
				irData = UxHwFloatUniformDist((float)tempInt - 0.5, (float)tempInt + 0.5) * gain;
			}
			else
			{
				irData = tempInt * gain;
			}

			kta = params->kta[pixelNumber] / ktaScale;
			kv = params->kv[pixelNumber] / kvScale;
			irData = irData - params->offset[pixelNumber] * (1 + kta * (ta - 25)) * (1 + kv * (vdd - 3.3));

			if (mode != params->calibrationModeEE)
			{
				irData = irData + params->ilChessC[2] * (2 * ilPattern - 1) - params->ilChessC[1] * conversionPattern;
			}

			irData = irData - params->tgc * irDataCP[subPage];
			irData = irData / emissivity;

			alphaCompensated = SCALEALPHA * alphaScale / params->alpha[pixelNumber];
			alphaCompensated = alphaCompensated * (1 + params->KsTa * (ta - 25));

			Sx = alphaCompensated * alphaCompensated * alphaCompensated * (irData + alphaCompensated * taTr);
			Sx = sqrt(sqrt(Sx)) * params->ksTo[1];
			To = sqrt(sqrt(irData / (alphaCompensated * (1 - params->ksTo[1] * 273.15) + Sx) + taTr)) - 273.15;

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
