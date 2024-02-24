#include "mdct.h"
//#include "test.h"
#include "costab.h"
#include <math.h>
#include <stddef.h>

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

static inline
float C(float a, float b) {
	return cosf(M_PI*(a / b));
}

static inline
float G(const float *in, size_t n) {
	return in[2*n];
}

static inline
float H(const float *in, size_t n, size_t blocklen) {
	return (n == 0)?            in[2*n + 1]
	     : (n == blocklen - 1)? in[2*n - 1]
	     :                      in[2*n + 1] + in[2*n - 1];
}

static inline
float u(const float *ubuf, size_t k, size_t blocklen) {
	float sum = 0;

	for (size_t n = 0; n < blocklen; n++) {
		//sum += ubuf[n] * C((2*n + 1/2.f + blocklen/2.f) * (k + 1/2.f), blocklen);
		sum += ubuf[n] * C((4*n + 1.f + blocklen) * (2*k + 1), 4*blocklen);
	}

	return sum;
}

/*
void plain_dct(const float *in, float *out, size_t blocklen) {
	for (size_t k = 0; k < blocklen; k++) {
		float sum = 0;

		for (size_t n = 0; n < 2*(blocklen - 1); n++) {
			sum += in[n] * C((n + 1/2.f + blocklen/2.f) * (k + 1/2.f), blocklen);
		}

		out[k] = sum;
	}
}
*/

void dct_decompose(const float *in, float *out, size_t blocklen) {
	float gbuf[blocklen];
	float hbuf[blocklen];

	float gout[blocklen/2];
	float hout[blocklen/2];

	for (size_t n = 0; n < blocklen; n++) {
		gbuf[n] = G(in, n);
		hbuf[n] = H(in, n, blocklen);
	}

	// TODO: there are very noticeable artifacts if this is any lower, something still broken?
	if (blocklen <= 16) {
		for (size_t k = 0; k < blocklen/2; k++) {
			gout[k] = u(gbuf, k, blocklen);
			hout[k] = u(hbuf, k, blocklen);
		}

	} else {
		//plain_dct(gbuf, gout, blocklen/2);
		//plain_dct(hbuf, hout, blocklen/2);
		dct_decompose(gbuf, gout, blocklen/2);
		dct_decompose(hbuf, hout, blocklen/2);
	}

	for (size_t i = 0; i < blocklen/2; i++) {
		float a = gout[i];
		float b = hout[i];
		float c = 1.f/(2.f*C(2*i + 1, 2*blocklen));

		out[i] = a + c*b;
		out[blocklen - i - 1] = a - c*b;
	}
}

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

	dct_decompose(norm, out, BLOCK_SIZE);

	/*
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
	*/
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

			//sum += val * cosf(M_PI/BLOCK_SIZE * samp);
			sum += val * dcttab(oidx, iidx);
		}

		for (uint iidx = 0; iidx < BLOCK_SIZE; iidx++) {
			float samp = (oidx + BLOCK_SIZE + 1/2.f + BLOCK_SIZE/2.f) * (iidx + 1/2.f);
			float val = block[iidx];

			//res += val * cosf(M_PI/(float)BLOCK_SIZE * samp);
			res += val * dcttab(oidx + BLOCK_SIZE, iidx);
		}

		float invN = 1.f/BLOCK_SIZE;
		float y = window(oidx, invN*sum) + inres[oidx];
		// TODO: 1.7 guesstimated, figure out proper multiple
		out[oidx] = clamp_i16(32767.f * 1.7 * y);
		outres[oidx] = window(oidx + BLOCK_SIZE, invN * res);
	}
}
