#pragma once

#include <stdint.h>

#define BLOCK_SIZE 1024

void mdct_encode(const int16_t block1[BLOCK_SIZE],
                 const int16_t block2[BLOCK_SIZE],
                 float out[BLOCK_SIZE]);
void mdct_decode(const float block[BLOCK_SIZE],
                 const float inres[BLOCK_SIZE],
                 int16_t out[BLOCK_SIZE],
                 float   outres[BLOCK_SIZE]);
