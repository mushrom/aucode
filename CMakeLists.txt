set(CMAKE_C_FLAGS "-lm -O3")
add_executable(encoder
	encode_main.c
	costab.c
	quant.c
	mdct.c)

add_executable(decoder
	decode_main.c
	costab.c
	quant.c
	mdct.c)

add_executable(costab_gen costab_gen.c)
