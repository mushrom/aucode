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

bool read_block_float(float foo[BLOCK_SIZE]) {
	size_t n = sizeof(float[BLOCK_SIZE]);
	int amt = fread_full(foo, n, stdin);
	return amt == n;
}

bool read_block_int16(int16_t foo[BLOCK_SIZE]) {
	size_t n = sizeof(int16_t[BLOCK_SIZE]);
	int amt = fread_full(foo, n, stdin);
	return amt == n;
}

bool write_block_s16le(int16_t foo[BLOCK_SIZE]) {
	size_t n = sizeof(int16_t[BLOCK_SIZE]);
	int amt = fwrite(foo, 1, n, stdout);
	return amt == n;
}

void expand_int16_to_float(const int16_t in[BLOCK_SIZE], float out[BLOCK_SIZE]) {
	for (unsigned i = 0; i < BLOCK_SIZE; i++) {
		out[i] = in[i];
	}
}

size_t read_block_compressed(uint8_t block[5*BLOCK_SIZE], float *factor) {
	uint32_t size;
	int ret;

	ret = fread_full(factor, 4, stdin);
	if (ret < 4) return false;

	ret = fread_full(&size, 4, stdin);
	//fprintf(stderr, "dec: reading %d\n", ret);
	if (ret < 4) return false;
	//fprintf(stderr, "dec: have %u\n", size);

	size = (size > 5*BLOCK_SIZE)? 5*BLOCK_SIZE : size;
	ret = fread_full(block, size, stdin);
	//fprintf(stderr, "dec: read %d\n", ret);
	if (ret < size) return false;

	return size;
}

void decompress_block(uint8_t in[5*BLOCK_SIZE], size_t size, int16_t out[BLOCK_SIZE]) {
	struct bitstream stream;
	bitstream_init(&stream, in);

	int16_t temp;
	for (unsigned i = 0; i < BLOCK_SIZE && bitstream_read(&stream, &temp); i++) {
		out[i] = temp;
	}
}

size_t decompress_rle(const uint8_t in[5*BLOCK_SIZE], size_t size, uint8_t out[5*BLOCK_SIZE]) {
	// TODO: proper bounds checking

	size_t oidx = 0;

	for (size_t i = 0; i < size; ) {
		if (in[i] == 0) {
			uint8_t count = in[i+1];
			uint8_t cur   = in[i+2];

			for (uint8_t k = 0; k < count; k++) {
				out[oidx + k] = cur;
			}

			oidx += count;
			i += 3;

		} else {
			out[oidx] = in[i];
			oidx++;
			i++;
		}
	}

	return oidx;
}

int main(void) {
	float in[BLOCK_SIZE];
	float resa[BLOCK_SIZE];
	float resb[BLOCK_SIZE];
	int16_t out[BLOCK_SIZE];

	float *cur  = resa;
	float *next = resb;

	memset(cur,  0, sizeof(resa));
	memset(next, 0, sizeof(resa));

	unsigned amt = 0;
	unsigned reduced = 0;

	int16_t ubuf[BLOCK_SIZE];
	uint8_t compbuf[5*BLOCK_SIZE];
	uint8_t rle[5*BLOCK_SIZE];
	size_t rsz;
	float v;

	for (unsigned i = 0; rsz = read_block_compressed(rle, &v); i++) {
		//fprintf(stderr, "have block: %lu bytes\n", rsz);
		size_t csz = decompress_rle(rle, rsz, compbuf);
		decompress_block(compbuf, csz, ubuf);
		//decompress_block(rle, rsz, ubuf);
		expand_int16_to_float(ubuf, in);
		unquant(in, v);
		mdct_decode(in, cur, out, next);

		float *temp = cur;
		cur = next;
		next = temp;

		write_block_s16le(out);
		//fprintf(stderr, "block %u (%u bbytes)\n", i, sizeof(int16_t[BLOCK_SIZE])*i);
	}

	return 0;
}
