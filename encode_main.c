#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "mdct.h"
#include "quant.h"
#include "bitstream.h"

int fread_full(void *ptr, size_t n, FILE *fp) {
	int idx = 0;
	int ret = 1;

	while (idx < n && ret > 0) {
		ret = fread((uint8_t*)ptr + idx, 1, n - idx, fp);
		idx += ret;
	}

	return idx;
}

bool read_block_s16le(int16_t foo[BLOCK_SIZE]) {
	size_t n = sizeof(int16_t[BLOCK_SIZE]);
	int amt = fread_full(foo, n, stdin);
	return amt == n;
}

bool write_block_float(float foo[BLOCK_SIZE]) {
	size_t n = sizeof(float[BLOCK_SIZE]);
	int amt = fwrite(foo, 1, n, stdout);
	return amt == n;
}

bool write_block_int16(int16_t foo[BLOCK_SIZE]) {
	size_t n = sizeof(int16_t[BLOCK_SIZE]);
	int amt = fwrite(foo, 1, n, stdout);
	return amt == n;
}

void condense_float_to_int16(const float in[BLOCK_SIZE], int16_t out[BLOCK_SIZE]) {
	for (unsigned i = 0; i < BLOCK_SIZE; i++) {
		if (in[i] > 32767 || in[i] < -32768) {
			fprintf(stderr, "NOTE: encoder: out of range: %g\n", in[i]);
		}

		int16_t temp = in[i];
		out[i] = temp;
	}
}

uint16_t compress_int16(const int16_t in[BLOCK_SIZE], uint8_t out[5*BLOCK_SIZE]) {
	struct bitstream stream;
	bitstream_init(&stream, out);

	for (unsigned i = 0; i < BLOCK_SIZE; i++) {
		bitstream_write(&stream, in[i]);
	}

	bitstream_flush(&stream);
	return stream.idx;
}

void write_compressed_block(const uint8_t block[5*BLOCK_SIZE], float *factor, uint32_t size) {
	// TODO: _full
	fwrite(factor, 4, 1, stdout);
	fwrite(&size, 4, 1, stdout);
	fwrite(block, 1, size, stdout);
}

int main(int argc, char* argv[]) {
	// sets scale factor for quantization,
	//   lower value -> heavier quantization, more compression, lower quality
	//   higher value -> lighter quantization, less compression, higher quality
	float quality_factor = 0.5;

	if (argc > 1) {
		quality_factor = atof(argv[1]);
	}

	int16_t bufa[BLOCK_SIZE];
	int16_t bufb[BLOCK_SIZE];
	float  out[BLOCK_SIZE];

	int16_t *cur  = bufa;
	int16_t *next = bufb;

	unsigned amt = 0;
	unsigned reduced = 0;

	read_block_s16le(cur);
	for (unsigned i = 0; read_block_s16le(next); i++) {
		int16_t buf[BLOCK_SIZE];
		uint8_t comp[5*BLOCK_SIZE];

		mdct_encode(cur, next, out);
		float v = quality_factor/sqrt(fmax(0.001, vol(out)));
		//float v = 32/diff_s16le(cur);
		//float v = 5000/vol_s16le(cur);
		//float v = 1;
		//fprintf(stderr, "enc: volume: %f\n", v);
		quant(out, v);

		amt += BLOCK_SIZE;
		reduced += count_zeros(out);

		condense_float_to_int16(out, buf);
		size_t sz = compress_int16(buf, comp);
		write_compressed_block(comp, &v, sz);
		//write_block_int16(buf);
		//write_block_float(out);

		int16_t *meh = cur;
		cur = next;
		next = meh;

		if (i % 20 == 0) {
			// debug output
			unsigned secs = (i * BLOCK_SIZE) / 44100;

			fprintf(stderr, "block %u @ %02u:%02u (%u bbytes)\n",
					i, secs / 60, secs % 60,
					sizeof(int16_t[BLOCK_SIZE])*i);
			fprintf(stderr, "samples: %u, reduced: %u\n", amt, reduced);
			fprintf(stderr, "%g%%\n", (float)reduced/(float)amt);

			amt /= 2;
			reduced /= 2;
		}

	}

	fprintf(stderr, "samples: %u, reduced: %u\n", amt, reduced);
	fprintf(stderr, "%g%%\n", (float)reduced/(float)amt);

	return 0;
}
