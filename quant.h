#pragma once
#include "mdct.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#define VOLUME_FACTORS 16

float quant(float block[BLOCK_SIZE], float v[VOLUME_FACTORS]);
float unquant(float block[BLOCK_SIZE], float v[VOLUME_FACTORS]);
void vol(float block[BLOCK_SIZE], float v[VOLUME_FACTORS]);
float vol_s16le(int16_t block[BLOCK_SIZE]);
float diff_s16le(int16_t block[BLOCK_SIZE]);
unsigned count_zeros(float block[BLOCK_SIZE]);
unsigned count_zeros_int16(int16_t block[BLOCK_SIZE]);

static inline
uint16_t compress_float_uint16(float v) {
	uint16_t ret = 0;
	int x;
	float n = frexpf(v, &x);

	if (fabs(v) == 0) {
		return 0;
	}

	if (abs(x) > 31) {
		fprintf(stderr, "NOTE: encoder: out of range: %g = %g^%d\n", v, n, x);
		x = (x < 0)? -31 : 31;
	}

	int sign = !!(x < 0 && x != -0);

	return (uint16_t)(sign << 15)
	     | (uint16_t)(((abs(x) & 0x1f) << 9))
	     | (uint16_t)(!!(n < 0) << 8)
	     | ((uint16_t)(0xff*fabs(n)) & 0xff)
	     ;
}

static inline
float uncompress_float_uint16(uint16_t in) {
	if (in == 0) {
		// special case for 0, don't exponentiate (0^0 = 1)
		return 0;
	}

	int x   = ((!!(in >> 15))? -1 : 1) * ((in >> 9) & 0x1f);
	float n = ((!!(in & 0x100))? -1 : 1) * (in & 0xff)/255.f;

	return ldexpf(n, x);
}
