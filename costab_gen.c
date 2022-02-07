#include <stdlib.h>
#include <math.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    unsigned block_size = 1024;

    if (argc > 1) {
        block_size = atoi(argv[1]);
    }

    printf("#ifndef _COSTAB_GEN_%d\n", block_size);
    printf("#define _COSTAB_GEN_%d\n", block_size);

    printf("float dct_table[%d][%d] = {", 2*block_size, block_size);
    for (int idx = 0; idx < 2*block_size; idx++) {
        printf("{");
        for (int odx = 0; odx < block_size; odx++) {
			float samp = (idx + 1/2.f + block_size/2.f) * (odx + 1/2.f);
            float val = cosf(M_PI/block_size * samp);

            printf("%f,", val);
        }

        printf("},\n");
    }

    printf("};\n");
    printf("#endif");
    return 0;
}
