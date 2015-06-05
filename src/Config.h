#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <pebble.h>


#ifndef PBL_COLOR
	#define BACKGROUND_COLOR GColorBlack
#else
	#define BACKGROUND_COLOR GColorCyan
#endif

#define FIRST_ROW_ELEMENT_BACKGROUND_COLOR GColorClear

#ifndef PBL_COLOR
	#define SECOND_ROW_ELEMENT_BACKGROUND_COLOR GColorWhite
#else
	#define SECOND_ROW_ELEMENT_BACKGROUND_COLOR GColorBrilliantRose
#endif

#define FIRST_ROW_TEXT_COLOR SECOND_ROW_ELEMENT_BACKGROUND_COLOR
#define SECOND_ROW_TEXT_COLOR BACKGROUND_COLOR

#endif