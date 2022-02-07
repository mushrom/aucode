#include "mdct.h"
//#include "test.h"
#include "costab.h"
#include <math.h>

typedef uint32_t uint;

static inline float clamp_i16(float k) {
	return fmax(-32768, fmin(32767, k));
}

// KBD window
static inline float window(unsigned n, float v) {
    float m = sinf(M_PI/(2.f*BLOCK_SIZE) * (n + 1/2.f));
    return v * sinf(M_PI/2.f * m*m);
}

/*
// MLT window
static inline float window(unsigned n, float v) {
    return v * sinf(M_PI/(2.f*BLOCK_SIZE) * (n + 1/2.f));
}
*/

/*
// no window
static inline float window(unsigned n, float v) {
    return v;
}
*/

void mdct_encode(const int16_t block1[BLOCK_SIZE],
                 const int16_t block2[BLOCK_SIZE],
                 float out[BLOCK_SIZE])
{
    // TODO: if not multithreading, could have this be static,
    //       if stack contrained... or pass this in as a parameter
    //       or, have an encoder context
	float norm[2*BLOCK_SIZE];

	for (uint i = 0; i < BLOCK_SIZE; i++) {
		norm[i]            = window(i,            block1[i] / 32768.0);
		norm[i+BLOCK_SIZE] = window(i+BLOCK_SIZE, block2[i] / 32768.0);
	}

	for (uint oidx = 0; oidx < BLOCK_SIZE; oidx++) {
		float sum = 0;

		for (uint iidx = 0; iidx < 2*BLOCK_SIZE; iidx++) {
			float samp = (iidx + 1/2.f + BLOCK_SIZE/2.f) * (oidx + 1/2.f);
			sum += norm[iidx] * cosf(M_PI/BLOCK_SIZE * samp);
            //sum += norm[iidx] * dct_table[iidx][oidx];
            //sum += norm[iidx] * dcttab(iidx, oidx);
		}

		out[oidx] = sum;
	}
}

void mdct_decode(const float block[BLOCK_SIZE],
                 const float inres[BLOCK_SIZE],
                 int16_t out[BLOCK_SIZE],
                 float   outres[BLOCK_SIZE])
{
	for (uint oidx = 0; oidx < BLOCK_SIZE; oidx++) {
		float sum = 0;
		float res = 0;

		for (uint iidx = 0; iidx < BLOCK_SIZE; iidx++) {
			float samp = ((float)oidx + 1/2.f + BLOCK_SIZE/2.f) * (iidx + 1/2.f);
			float val = block[iidx];

			sum += val * cosf(M_PI/BLOCK_SIZE * samp);
			//sum += val * dcttab(oidx, iidx);
		}

		for (uint iidx = 0; iidx < BLOCK_SIZE; iidx++) {
			float samp = (oidx + BLOCK_SIZE + 1/2.f + BLOCK_SIZE/2.f) * (iidx + 1/2.f);
			float val = block[iidx];

			res += val * cosf(M_PI/(float)BLOCK_SIZE * samp);
			//res += val * dcttab(oidx + BLOCK_SIZE, iidx);
		}

		float invN = 1.f/BLOCK_SIZE;
		float y = window(oidx, invN*sum) + inres[oidx];
		// TODO: 1.7 guesstimated, figure out proper multiple
		out[oidx] = clamp_i16(32767.f * 1.7 * y);
		outres[oidx] = window(oidx + BLOCK_SIZE, invN * res);
	}
}
