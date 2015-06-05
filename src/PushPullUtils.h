#ifndef __PUSH_PULL_UTILS_H__
#define __PUSH_PULL_UTILS_H__

#include "Config.h"

#define MAKE_UPDATE_FN_NAME(x) update_ ## x ## _layer
#define MAKE_UPDATE_FN_NAME_N(x, n) update_ ## x ## _layer ## _ ##n

#define MAKE_UPDATE_TEXT_LAYER(layer, n, initial_buffer) void MAKE_UPDATE_FN_NAME_N(layer, n) (){ \
	static char buffer[] = initial_buffer; \
	MAKE_UPDATE_FN_NAME(layer)(layer ## _ ## n, buffer, sizeof(buffer)); }
	
#define MAKE_UPDATE_TEXT_LAYER_PAIR(layer, initial_buffer) \
	MAKE_UPDATE_TEXT_LAYER(layer, 1, initial_buffer) \
	MAKE_UPDATE_TEXT_LAYER(layer, 2, initial_buffer) 

#define ROW_BOOTSTRAP_BEGIN(x) { int cl_i = 0; int total = x;
#define ROW_ADD_LAYER(row_ptr, window, layer1, layer2, rsp) row_ptr = initialize_row_n_out_of(window, layer1, layer2, rsp, cl_i, total); \
	++cl_i
	
#define ROW_BOOTSTRAP_END }

#define MAKE_TEXT_LAYER_N(layer, n, font) layer ## _ ## n = text_layer_create(GRectZero); \
	text_layer_set_background_color(layer ## _ ## n, n % 2 ? FIRST_ROW_ELEMENT_BACKGROUND_COLOR : SECOND_ROW_ELEMENT_BACKGROUND_COLOR); \
  	text_layer_set_text_color(layer ## _ ## n, n % 2 ? FIRST_ROW_TEXT_COLOR : SECOND_ROW_TEXT_COLOR); \
  	text_layer_set_font(layer ## _ ## n, font); \
  	text_layer_set_text_alignment(layer ## _ ## n, GTextAlignmentCenter)
	  
#define MAKE_TEXT_LAYER_PAIR(layer, font) \
	MAKE_TEXT_LAYER_N(layer, 1, font);	\
	MAKE_TEXT_LAYER_N(layer, 2, font)

#endif