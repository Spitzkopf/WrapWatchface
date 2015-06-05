#ifndef __PUSH_PULL_UTILS_H__
#define __PUSH_PULL_UTILS_H__

#define MAKE_UPDATE_FN_NAME(x) update_ ## x ## _layer
#define MAKE_UPDATE_FN_NAME_N(x, n) update_ ## x ## _layer ## _ ##n

#define MAKE_UPDATE_TEXT_LAYER(layer, n, initial_buffer) void MAKE_UPDATE_FN_NAME_N(layer, n) (){ \
	static char buffer[] = initial_buffer; \
	MAKE_UPDATE_FN_NAME(layer)(layer ## _ ## n, buffer, sizeof(buffer)); }
	
#define MAKE_UPDATE_TEXT_LAYER_PAIR(layer, initial_buffer) \
	MAKE_UPDATE_TEXT_LAYER(layer, 1, initial_buffer) \
	MAKE_UPDATE_TEXT_LAYER(layer, 2, initial_buffer) 

#endif