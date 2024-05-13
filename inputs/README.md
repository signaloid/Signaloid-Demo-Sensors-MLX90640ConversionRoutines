# Inputs

This folder holds the input data for the MLX90640 conversion routines.

- `EEPROM-calibration-data.csv`: MLX90640 EEPROM calibration data in comma-separated form.
- `raw-frame-data.csv`: Three raw frame captures (before applying any conversion routines) in comma-separated form. 

The values in `EEPROM-calibration-data.csv` are the results of a `getEE()` call from the MLX library.
The values in `raw-frame-data.csv` are the results of a `getFrameAndRaw()` call from the MLX library.
