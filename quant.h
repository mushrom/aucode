#pragma once
#include "mdct.h"

float quant(float block[BLOCK_SIZE], float v);
float unquant(float block[BLOCK_SIZE], float v);
float vol(float block[BLOCK_SIZE]);
float vol_s16le(int16_t block[BLOCK_SIZE]);
float diff_s16le(int16_t block[BLOCK_SIZE]);
unsigned count_zeros(float block[BLOCK_SIZE]);
