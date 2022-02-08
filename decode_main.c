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

struct uint8_channels {
	uint32_t channels;
	// pointer to int16_t[channels][5*BLOCK_SIZE]
	uint8_t **blocks;
	uint32_t *sizes;
};

// TODO: header
struct int16_channels {
	uint32_t channels;
	// pointer to int16_t[channels][BLOCK_SIZE]
	int16_t **blocks;
};

// TODO: header
struct float_channels {
	uint32_t channels;
	// pointer to float_t[channels][BLOCK_SIZE]
	float_t **blocks;
};

void init_uint8_channels(struct uint8_channels *chans, uint32_t n) {
	chans->channels = n;
	chans->blocks = calloc(1, sizeof(uint8_t*[n]));
	chans->sizes  = calloc(1, sizeof(uint32_t[n]));

	for (uint32_t i = 0; i < n; i++) {
		chans->blocks[i] = calloc(1, sizeof(uint8_t[5*BLOCK_SIZE]));
	}
}

struct uint8_channels *create_uint8_channels(uint32_t n) {
	struct uint8_channels *ret = malloc(sizeof(struct uint8_channels));

	if (!ret) return NULL;

	init_uint8_channels(ret, n);
	return ret;
}

void init_int16_channels(struct int16_channels *chans, uint32_t n) {
	chans->channels = n;
	chans->blocks = calloc(1, sizeof(int16_t*[n]));

	for (uint32_t i = 0; i < n; i++) {
		chans->blocks[i] = calloc(1, sizeof(int16_t[BLOCK_SIZE]));
	}
}

struct int16_channels *create_int16_channels(uint32_t n) {
	struct int16_channels *ret = malloc(sizeof(struct int16_channels));

	if (!ret) return NULL;

	init_int16_channels(ret, n);
	return ret;
}

void init_float_channels(struct float_channels *chans, uint32_t n) {
	chans->channels = n;
	chans->blocks = calloc(1, sizeof(float*[n]));

	for (uint32_t i = 0; i < n; i++) {
		chans->blocks[i] = calloc(1, sizeof(float[BLOCK_SIZE]));
	}
}

struct float_channels *create_float_channels(uint32_t n) {
	struct float_channels *ret = malloc(sizeof(struct float_channels));

	if (!ret) return NULL;

	init_float_channels(ret, n);
	return ret;
}

bool read_channel_blocks_compressed(struct uint8_channels *buf, float *factors) {
	for (uint32_t c = 0; c < buf->channels; c++) {
		int amt;

		amt = fread_full(factors + c, 4, stdin);
		if (amt != 4) return false;

		amt = fread_full(buf->sizes + c, 4, stdin);
		if (amt != 4) return false;

		amt = fread_full(buf->blocks[c], buf->sizes[c], stdin);
		if (amt != buf->sizes[c]) return false;
	}

	return true;
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

bool write_block_s16le_interleaved(struct int16_channels *foo) {
	for (unsigned i = 0; i < BLOCK_SIZE; i++) {
		for (uint32_t c = 0; c < foo->channels; c++) {
			int amt = fwrite(foo->blocks[c] + i, 1, 2, stdout);
			if (amt != 2) return false;

			//size_t n = sizeof(int16_t[BLOCK_SIZE]);
			//int amt = fwrite(foo, 1, n, stdout);
			//return amt == n;
		}
	}

	return true;
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
	//float in[BLOCK_SIZE];
	//float resa[BLOCK_SIZE];
	//float resb[BLOCK_SIZE];
	//int16_t out[BLOCK_SIZE];
	
	//memset(cur,  0, sizeof(resa));
	//memset(next, 0, sizeof(resa));

	unsigned amt = 0;
	unsigned reduced = 0;

	//uint8_t rle[5*BLOCK_SIZE];
	size_t rsz;
	float v[2];

	struct float_channels *in   = create_float_channels(2);
	struct float_channels *resa = create_float_channels(2);
	struct float_channels *resb = create_float_channels(2);
	struct int16_channels *out  = create_int16_channels(2);

	struct uint8_channels *rle  = create_uint8_channels(2);
	//struct uint8_channels *compbuf = create_uint8_channels(2);

	struct float_channels *cur  = resa;
	struct float_channels *next = resb;

	//for (unsigned i = 0; rsz = read_block_compressed(rle, &v); i++) {
	for (unsigned i = 0; rsz = read_channel_blocks_compressed(rle, v); i++) {
		for (uint32_t chan = 0; chan < rle->channels; chan++) {
			int16_t ubuf[BLOCK_SIZE];
			uint8_t compbuf[5*BLOCK_SIZE];

			//fprintf(stderr, "have block: %lu bytes\n", rsz);
			size_t csz = decompress_rle(rle->blocks[chan], rle->sizes[chan], compbuf);
			decompress_block(compbuf, csz, ubuf);
			//decompress_block(rle, rsz, ubuf);
			expand_int16_to_float(ubuf, in->blocks[chan]);
			unquant(in->blocks[chan], v[chan]);
			mdct_decode(in->blocks[chan],  cur->blocks[chan],
			            out->blocks[chan], next->blocks[chan]);

		}

		//float *temp = cur;
		void *temp = cur;
		cur = next;
		next = temp;

		write_block_s16le_interleaved(out);
		//write_block_s16le(out->blocks[0]);
		//fprintf(stderr, "block %u (%u bbytes)\n", i, sizeof(int16_t[BLOCK_SIZE])*i);
	}

	return 0;
}
