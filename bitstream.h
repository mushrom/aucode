#pragma once
#include <assert.h>
#include <stdlib.h>

struct bitstream {
	uint8_t *ptr;
	size_t idx;
	unsigned count;
	uint8_t buf;
};

static inline
void bitstream_init(struct bitstream *str, uint8_t *ptr) {
	str->ptr = ptr;
	str->idx = 0;
	str->count = 0;
	str->buf = 0;
}

static inline
void bitstream_write_bit(struct bitstream *str, bool b) {
    str->buf |= b << str->count;
    str->count++;

	if (str->count == 8) {
		str->ptr[str->idx] = str->buf;
		str->idx++;
		str->count = 0;
        str->buf = 0;
	}
}

static inline
bool bitstream_read_bit(struct bitstream *str) {
    if (str->count == 0) {
        str->buf = str->ptr[str->idx];
        str->count = 8;
        str->idx++;
    }

    bool ret = str->buf & 1;
    str->buf >>= 1;
    str->count -= 1;

    return ret;
}

static inline
void bitstream_add_nibble(struct bitstream *str, uint8_t nib) {
    for (unsigned i = 0; i < 4; i++) {
        bitstream_write_bit(str, nib & (1<<i));
    }
}

static inline
uint8_t bitstream_get_nibble(struct bitstream *str) {
    uint8_t ret = 0;

    for (unsigned i = 0; i < 4; i++) {
        ret |= bitstream_read_bit(str) << i;
    }

    return ret;
}

static inline
void bitstream_write(struct bitstream *str, int16_t v) {
    //int16_t value = v;
    //uint16_t k = *(uint16_t*)&value;
    bool sign = v < 0;
    bool zero = v == 0;

    bitstream_write_bit(str, zero);

    if (zero) return;

    bitstream_write_bit(str, sign);

    uint16_t k = abs(v);
    //assert(*(int16_t*)&k == v);

    while (k > 7) {
        bitstream_add_nibble(str, 8 | (k & 7));
        k >>= 3;
    }

    bitstream_add_nibble(str, k&7);
}

static inline
void bitstream_flush(struct bitstream *str) {
    if (str->count) {
        unsigned n = 8 - str->count;
        for (unsigned i = 0; i < n; i++) {
            bitstream_write_bit(str, 0);
        }
    }
}

static inline
bool bitstream_read(struct bitstream *str, int16_t *value) {
    bool sign;
    bool zero;

    zero = bitstream_read_bit(str);

    if (zero) {
        *value = 0;
        return true;
    }

    sign = bitstream_read_bit(str);

    uint16_t temp = 0;
    uint16_t nib = bitstream_get_nibble(str);

    unsigned n = 0;
    while (nib & 8) {
        temp |= (nib & 7) << n;
        n += 3;
        nib = bitstream_get_nibble(str);
    }

    temp |= (nib&7) << n;

    //*value = *(int16_t*)&temp;
    int s = sign? -1 : 1;
    *value = s * temp;
    //assert(*(int16_t*)&temp == *value);

    return true;
}

