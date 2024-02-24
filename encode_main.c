#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "mdct.h"
#include "quant.h"
#include "bitstream.h"

static inline float smoothstep(float v) {
	return 3*v*v - 2*v*v*v;
}

static inline float mix(float a, float b, float amt) {
	return (1.0 - amt)*a + amt*b;
}

int fread_full(void *ptr, size_t n, FILE *fp) {
	int idx = 0;
	int ret = 1;

	while (idx < n && ret > 0) {
		ret = fread((uint8_t*)ptr + idx, 1, n - idx, fp);
		idx += ret;
	}

	return idx;
}

struct int16_channels {
	uint32_t channels;
	// pointer to int16_t[channels][BLOCK_SIZE]
	int16_t **blocks;
};

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

bool read_channel_blocks_s16le(struct int16_channels *buf) {
	for (unsigned i = 0; i < BLOCK_SIZE; i++) {
		for (uint32_t n = 0; n < buf->channels; n++) {
			int amt = fread_full(buf->blocks[n] + i, 2, stdin);

			if (amt != 2) {
				// TODO: error
				return false;
			}
		}
	}

	return true;
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

		int16_t temp = round(in[i]);
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

size_t compress_rle(const uint8_t in[BLOCK_SIZE], size_t size, uint8_t out[5*BLOCK_SIZE]) {
	size_t oidx = 0;

	for (size_t i = 0; i < size; ) {
		uint8_t cur = in[i];
		uint8_t count = 1;

		for (size_t k = i+1; k < size && k - i < 254; k++) {
			if (in[k] == cur) {
				count++;
			} else break;
		}

		if (count < 3 && cur != 0) {
			out[oidx] = in[i];
			oidx++;
			i++;

		} else {
			out[oidx]     = 0;
			out[oidx + 1] = count;
			out[oidx + 2] = cur;
			oidx += 3;
			i += count;
		}
	}

	return oidx;
}

void write_compressed_block(const uint8_t block[5*BLOCK_SIZE], uint16_t volumes[VOLUME_FACTORS], uint32_t size) {
	// TODO: _full
	// TODO: endianness
	fwrite(volumes, sizeof(uint16_t[VOLUME_FACTORS]), 1, stdout);
	fwrite(&size, 4, 1, stdout);
	fwrite(block, 1, size, stdout);
}

void box_blur(const float in[BLOCK_SIZE], float out[BLOCK_SIZE], int width) {
	for (int i = 0; i < BLOCK_SIZE; i++) {
		float sum = 0;
		float count = 0;

		for (int k = -width; k <= width; k++) {
			if (i + k < 8 || i + k >= BLOCK_SIZE) {
				count += 1;
				continue;
			}

			sum   += fabs(in[i]);
			count += 1;
		}

		out[i] = sum/count;
	}
}

void linear_blur(const float in[BLOCK_SIZE], float out[BLOCK_SIZE], int width) {
	for (int i = 0; i < BLOCK_SIZE; i++) {
		float sum = 0;
		float count = 0;

		for (int k = -width; k <= width; k++) {
			if (i + k < 8 || i + k >= BLOCK_SIZE) {
				count += 1;
				continue;
			}

			float v = fabs(k)/width;
			sum   += v * fabs(in[i]);
			count += 1;
		}

		out[i] = sum/count;
	}
}

void blur_harmonics(const float in[BLOCK_SIZE], float out[BLOCK_SIZE], int width) {
	for (int i = 0; i < BLOCK_SIZE; i++) {
		float sum = 0;
		float count = 0;

		if (i < 4) {
			out[i] = 0;
			continue;
		}

		//for (int k = -width; k < 0; k++) {
		//for (int k = -width; k < width; k++) {
		for (int k = 0; k < width; k++) {
			if (k < 0 && (i >> -k) < 32)
				continue;

			if (k > 0 && (i << k) >= BLOCK_SIZE)
				continue;

			float v = fabs(k)/width;
			sum   += v*fabs(in[(k < 0)? (i >> -k) : (i << k)]);
			count += 1;
		}

		if (count == 0) {
			out[i] = 0;

		} else {
			out[i] = sum/count;
		}
	}
}

float sign(float n) {
	return (n < 0)? -1 : 1;
}

void freq_curve(float out[BLOCK_SIZE], float base, float mult, float offset) {
	for (unsigned i = 0; i < BLOCK_SIZE; i++) {
		out[i] = offset + base + (1.0 - base)*mult*(float)i/BLOCK_SIZE;
	}
}

void thresh(const float in[BLOCK_SIZE], const float buf[BLOCK_SIZE], float out[BLOCK_SIZE], float level) {
	for (unsigned i = 0; i < BLOCK_SIZE; i++) {
		float k = fabs(in[i]) - level*buf[i];

		if (round(k) > 0) {
			out[i] = in[i];

		} else {
			out[i] = 0;
		}
	}
}

void sub(const float in[BLOCK_SIZE], const float buf[BLOCK_SIZE], float out[BLOCK_SIZE], float level) {
	for (unsigned i = 0; i < BLOCK_SIZE; i++) {
		float k = in[i] - level*buf[i];
		if (!!signbit(k) == !!signbit(in[i])) {
			out[i] = k;
		} else {
			out[i] = 0;
		}
	}
}

int main(int argc, char* argv[]) {
	// sets scale factor for quantization,
	//   higher value -> heavier quantization, more compression, lower quality
	//   lower value  -> lighter quantization, less compression, higher quality
	float quality_factor = 1.0;

	if (argc > 1) {
		quality_factor = atof(argv[1]);
	}

	unsigned lines = 1024;
	unsigned curLine = 0;
	FILE *fp = fopen("debug-in.ppm", "w");
	fprintf(fp, "P3\n%u %u\n255\n", BLOCK_SIZE, lines);

	unsigned amt = 0;
	unsigned reduced = 0;

	struct int16_channels *cur  = create_int16_channels(2);
	struct int16_channels *next = create_int16_channels(2);

	read_channel_blocks_s16le(cur);
	for (unsigned i = 0; read_channel_blocks_s16le(next); i++) {
		for (uint32_t chan = 0; chan < cur->channels; chan++) {
			int16_t buf[BLOCK_SIZE];
			uint8_t comp[5*BLOCK_SIZE];
			uint8_t rle[5*BLOCK_SIZE];
			float  out[BLOCK_SIZE];

			mdct_encode(cur->blocks[chan], next->blocks[chan], out);

#ifdef DEBUG_SPECTRUM
			if (chan == 0 && curLine < lines) {
				for (size_t k = 0; k < BLOCK_SIZE; k++) {
					//int n = sqrt(fabs(out[k])) * 10;
					int n = fmin(255, fabs(out[k]) * 32);
					int v = fabs(out[k]) * 8;
					fprintf(fp, "%d %d %d ", v, n, n);
				}
				fprintf(fp, "\n");
				fflush(fp);
				curLine++;
			}

			if (fp && curLine >= lines) {
				fclose(fp);
				fp = NULL;
			}
#endif

			float boxblur[BLOCK_SIZE];
			float linblur[BLOCK_SIZE];
			float harmonics[BLOCK_SIZE];

			float volumes[VOLUME_FACTORS];

			float qf = 1.0/quality_factor;
			float freq[BLOCK_SIZE];
			vol(out, volumes);

			float volsum = 0;
			for (size_t i = 1; i < VOLUME_FACTORS; i++) {
				volsum += volumes[i];
			}
			volsum /= VOLUME_FACTORS;
			volsum = sqrt(volsum);

			//fprintf(stderr, "%g: ", volsum);
			for (size_t i = 0; i < VOLUME_FACTORS; i++) {
				volumes[i] *= quality_factor*volsum;
				//volumes[i] *= volsum;
				//volumes[i] *= quality_factor;
				volumes[i] = sqrt(volumes[i]);
				//volumes[i] = fmin(10, volumes[i]);
				//fprintf(stderr, "%g ", volumes[i]);
			}

			for (size_t i = 1; i < VOLUME_FACTORS; i++) {
				// XXX: make sure we're using the exact same values in encoder and decoder
				volumes[i] = uncompress_float_uint16(compress_float_uint16(volumes[i]));
			}

#define DEBUG_VOLUMES 1
#ifdef DEBUG_VOLUMES
			if (chan == 0 && curLine < lines) {
				for (size_t k = 0; k < BLOCK_SIZE; k++) {
					//int n = sqrt(fabs(out[k])) * 10;
					size_t meh = BLOCK_SIZE / VOLUME_FACTORS;
					float amt = (float)k/meh - k/meh;
					size_t hmm = (k/meh + 1 >= VOLUME_FACTORS)? VOLUME_FACTORS - 1 : k/meh + 1;
					//float samp = mix(volumes[k/meh], volumes[hmm], smoothstep(amt));
					float samp = mix(volumes[k/meh], volumes[hmm], amt);
					int n = fmin(255, fabs(samp * 32));
					int v = fabs(samp) * 8;
					fprintf(fp, "%d %d %d ", v, n, n);
				}
				fprintf(fp, "\n");
				fflush(fp);
				curLine++;
			}

			if (fp && curLine >= lines) {
				fclose(fp);
				fp = NULL;
			}
#endif

			//fprintf(stderr, "\n");
			quant(out, volumes);

			/*
			freq_curve(freq, 0.0, 0.33, 0.0);
			thresh(out, freq, out, 0.5);
			*/

			blur_harmonics(out, harmonics, 4);
			//thresh(out, harmonics, out, 0.66);
			thresh(out, harmonics, out, 1.0);

			//box_blur(out, boxblur, 16);
			//thresh(out, boxblur, out, 0.05);
			//linear_blur(out, linblur, 8);
			//thresh(out, linblur, out, 0.1);
			//sub(out, blurred, out, 1.0);

			amt += BLOCK_SIZE;

			condense_float_to_int16(out, buf);
			reduced += count_zeros_int16(buf);
			size_t csz = compress_int16(buf, comp);
			size_t rsz = compress_rle(comp, csz, rle);

			uint16_t volume_comp[VOLUME_FACTORS];
			for (size_t k = 0; k < VOLUME_FACTORS; k++) {
				volume_comp[k] = compress_float_uint16(volumes[k]);
			}
			write_compressed_block(rle, volume_comp, rsz);
			//write_compressed_block(comp, &v, csz);
			//write_block_int16(buf);
			//write_block_float(out);
		}

		if (true && i % 20 == 0) {
			// debug output
			unsigned secs = (i * BLOCK_SIZE) / 44100;

			fprintf(stderr, "block %u @ %02u:%02u (%u bytes)\n",
					i, secs / 60, secs % 60,
					sizeof(int16_t[BLOCK_SIZE])*i);
			fprintf(stderr, "samples: %u, reduced: %u\n", amt, reduced);
			fprintf(stderr, "%g%%\n", 100 * (float)reduced/(float)amt);

			amt = 0;
			reduced = 0;
		}
		//struct int16_channels *meh = cur;
		void *meh = cur;
		cur = next;
		next = meh;
	}

	fprintf(stderr, "samples: %u, reduced: %u\n", amt, reduced);
	fprintf(stderr, "%g%%\n", (float)reduced/(float)amt);

	return 0;
}
