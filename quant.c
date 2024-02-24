#include "quant.h"
#include "mdct.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static float smoothstep(float v) {
	return 3*v*v - 2*v*v*v;
}

static float mix(float a, float b, float amt) {
	return (1.0 - amt)*a + amt*b;
}

void vol(float block[BLOCK_SIZE], float v[VOLUME_FACTORS]) {
	size_t inc = BLOCK_SIZE / VOLUME_FACTORS;

	for (size_t n = 0; n < VOLUME_FACTORS; n++) {
		float sum = 0;

		for (size_t i = n * inc; i < (n+1) * inc; i++) {
			sum += fabs(block[i]);
		}

		v[n] = sum/inc;
	}
}

// not used, leaving for future experimentation
float vol_s16le(int16_t block[BLOCK_SIZE]) {
	float sum = 0;

	for (unsigned n = 0; n < BLOCK_SIZE; n++) {
		sum += fabs(block[n]);
	}

	return sum / BLOCK_SIZE;
}

// not used, leaving for future experimentation
#define DIV 16
float diff_s16le(int16_t block[BLOCK_SIZE]) {
    float avg[BLOCK_SIZE / DIV];
    float diff[BLOCK_SIZE / DIV];
	float sum = 0;

    memset(avg, 0, sizeof(avg));
    memset(diff, 0, sizeof(diff));

	for (unsigned n = 0; n < BLOCK_SIZE; n += DIV) {
        float m = 0;
        for (unsigned k = 0; k < DIV; k++) {
            m += fabs(block[n])/DIV;
        }

        avg[n/DIV] = m;

	}

    for (unsigned n = 0; n < BLOCK_SIZE/DIV-1; n++) {
        diff[n] = logf(M_E + fabs(avg[n] - avg[n+1]));
    }

    /*
    for (unsigned i = 0; i < BLOCK_SIZE/DIV; i++) {
        fprintf(stderr, "avg[%d]: %f\n", i, avg[i]);
    }

    for (unsigned i = 0; i < BLOCK_SIZE/DIV; i++) {
        fprintf(stderr, "diff[%d]: %f, %f\n", i, diff[i], logf(diff[i]));
    }
    sleep(1);
    */

    for (unsigned n = 0; n < BLOCK_SIZE/DIV - 1; n++) {
        sum += diff[n];
    }

    return sum/DIV;
}

float cur_volume(size_t n, float v[VOLUME_FACTORS]) {
	size_t meh = BLOCK_SIZE / VOLUME_FACTORS;
	float amt = (float)n/meh - n/meh;
	size_t hmm = (n/meh + 1 >= VOLUME_FACTORS)? VOLUME_FACTORS - 1 : n/meh + 1;
	//float samp = mix(v[n/meh], v[hmm], smoothstep(amt));
	float samp = mix(v[n/meh], v[hmm], amt);
	return samp;
}

float quant(float block[BLOCK_SIZE], float v[VOLUME_FACTORS]) {
    float fac[BLOCK_SIZE];
	size_t inc = BLOCK_SIZE / VOLUME_FACTORS;

	for (unsigned n = 0; n < BLOCK_SIZE; n++) {
		float curvol = cur_volume(n, v);
		block[n] = (curvol == 0)? 0.f : block[n]/curvol;
	}
}

float unquant(float block[BLOCK_SIZE], float v[VOLUME_FACTORS]) {
	size_t inc = BLOCK_SIZE / VOLUME_FACTORS;

	for (unsigned n = 0; n < BLOCK_SIZE; n++) {
		float curvol = cur_volume(n, v);
		block[n] = block[n]*curvol;
		//block[n] = block[n]*v[n / inc];
	}
}

unsigned count_zeros(float block[BLOCK_SIZE]) {
	unsigned ret = 0;
	for (unsigned n = 0; n < BLOCK_SIZE; n++) {
		ret += ((unsigned)(fabs(block[n])) == 0);
	}

	return ret;
}

unsigned count_zeros_int16(int16_t block[BLOCK_SIZE]) {
	unsigned ret = 0;
	for (unsigned n = 0; n < BLOCK_SIZE; n++) {
		ret += block[n] == 0;
	}

	return ret;
}
