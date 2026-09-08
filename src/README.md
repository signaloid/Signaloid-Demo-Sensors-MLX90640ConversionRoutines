# Source code:

## main.c
Entry point: parses command-line arguments, dispatches to the UxHw or Monte Carlo kernel, and prints/saves results.

## kernel.*
Implementation of the MLX90640 conversion routines (adapted from the Melexis original library to model ADC quantization error).

## mlx90640-uxhw.*
UxHw-mode calculation kernel: a single distributional evaluation of the conversion routine.

## mlx90640-monte-carlo.*
Monte Carlo-mode calculation kernel: runs the native Monte Carlo simulation loop.

## common.*
Signaloid common utility routines.

## utilities.*
Utilities for parsing command-line arguments and handling I/O.

## MLX90640_API.*, MLX90640_I2C_Driver.h
Vendored MLX90640 driver from the sensor's manufacturer, Melexis.

## mlx90640-i2c.c
Empty functions to satisfy the requirements for the MLX90640 library.

## uxhw.*
UxHw compatibility shim used to build and run the native Monte Carlo executable.

## config.mk
Configuration options to customize build for running on Signaloid Cloud Compute Engine.
