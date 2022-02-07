#include "quant.h"
#include "mdct.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

float vol(float block[BLOCK_SIZE]) {
	float sum = 0;

	for (unsigned n = 0; n < BLOCK_SIZE; n++) {
		//float m = fabs(block[n]);
		float m = fabs(block[n]) * (float)n/BLOCK_SIZE;
		//float m = fabs(block[n]) * powf((float)n/BLOCK_SIZE, 3);
        sum += m;
	}

	//return sum / BLOCK_SIZE; // flat
	return sum / 511.5; // linear
	//return sum / 341.f; // pow 2
	//return sum / 255.f; // pow 3
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

void dim_harmonics(float block[BLOCK_SIZE], float fac[BLOCK_SIZE]) {
    for (unsigned n = 0; n < BLOCK_SIZE; n++) {
        fac[n] = 1.0;
    }

    for (unsigned n = 2; n < BLOCK_SIZE; n++) {
        for (unsigned k = n*2; k < BLOCK_SIZE; k *= 2) {
            //fac[k] += block[n] * ((float)k/BLOCK_SIZE);
            //fac[k] += block[n] * powf((float)k/BLOCK_SIZE, 2);
            //fac[k] += block[n] * sqrtf((float)k/BLOCK_SIZE);

            // amplifying opposite sign freqs and dimming same sign seems to work ok
            float a = (block[n] > 0)? 1 : -1;
            float b = (block[k] > 0)? 1 : -1;

            //fac[k] += (a*b)*(block[n] * powf(((float)(k)/BLOCK_SIZE), 2));
            fac[k] += (a*b)*(block[n] * powf(((float)(k-n)/BLOCK_SIZE), 2));
            //fac[k] += block[n] * powf(((float)(k-n)/BLOCK_SIZE), 2);
        }
    }
}

float quant(float block[BLOCK_SIZE], float v) {
    float fac[BLOCK_SIZE];

    dim_harmonics(block, fac);

	for (unsigned n = 0; n < BLOCK_SIZE; n++) {
        float sign = (block[n] < 0)? -1 : 1;
        block[n] = fabs(block[n]);
        //block[n] = round(round(v*block[n]/fac[n]) * fac[n]);
        block[n] = round(v*block[n]);
        block[n] *= sign;
	}
}

float unquant(float block[BLOCK_SIZE], float v) {
	for (unsigned n = 0; n < BLOCK_SIZE; n++) {
        block[n] = block[n]/v;
	}
}

unsigned count_zeros(float block[BLOCK_SIZE]) {
	unsigned ret = 0;
	for (unsigned n = 0; n < BLOCK_SIZE; n++) {
		ret += ((unsigned)(fabs(block[n])) == 0);
	}

	return ret;
}
