#define RENDER_TEST1
#ifdef RENDER_TEST
#include <nanovg.h>
#include "color_util.h"
#include "seq_math.h"
#include "logging.h"
#include "platform.h"

int mainTest(void (*drawFn)(NVGcontext*,int,int,float));
int offsetX = 0;
int offsetY = 0;
void testAAScissorBleeding(NVGcontext* ctx, int winW, int winH, float pxratio) {
	offsetX = (getTimeMillis()/5)%500;
	nvgBeginFrame(ctx, winW, winH, pxratio);
	nvgTranslate(ctx, offsetX, offsetY);
	nvgSave(ctx);
		nvgTranslate(ctx, 5, 5);
		nvgIntersectScissor(ctx, 0, 0, 50, 50);
		nvgBeginPath(ctx);
		nvgRect(ctx, 0, 0, 50, 50);
		nvgFillColor(ctx, rgbToNvg(0xff00ff));
		nvgFill(ctx);
	nvgRestore(ctx);
	nvgIntersectScissor(ctx, 0, 0, 25, 25);
	nvgSave(ctx);
		nvgTranslate(ctx, 0, 30);
		nvgIntersectScissor(ctx, 0, 0, 50, 50);
		nvgBeginPath(ctx);
		nvgRect(ctx, 0, 0, 50, 50);
		nvgFillColor(ctx, rgbToNvg(0xffffff));
		nvgFill(ctx);
	nvgRestore(ctx);
	nvgEndFrame(ctx);
}

int main(int argc, char* argv[]) {
//	testTickConversions();
	
	return mainTest(testAAScissorBleeding);
}

#else
int mainHost(int argc, char* argv[]);
int main(int argc, char* argv[]) {
	return mainHost(argc, argv);
}
#endif
