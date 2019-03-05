#pragma once
#include "fileio.h"
#define ICON_SYNTH 0
#define ICON_EFFECT 1
#define ICON_FOLDER 2
#define ICON_FOLDER_OPEN 3
#define ICON_FILE 4
#define ICON_COPY 5
#define ICON_PASTE 6
#define ICON_CUT 7
#define ICON_ADJUST 8
#define ICON_CLOSE 9
#define ICON_BYPASS 10
#define ICON_LOOP 11
#define IMG_LED 12
#define IMG_LED_OFF 13
#define IMG_LED_GLOW 14
#define ICON_ARR_DOWN 15
#define ICON_ARR_LEFT 16
#define ICON_ARR_RIGHT 17
#define ICON_PLUS 18
#define ICON_MINUS 19
#define ICON_AUTOMATION 20
#define NUM_IMGS 21

namespace RenderResources {
	struct NvgImageTexture {
		int id;
		int width;
		int height;
	};
	extern NvgImageTexture imgDashedLine;
	extern NvgImageTexture imgIcons[NUM_IMGS];
};
