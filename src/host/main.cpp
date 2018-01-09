#define RENDER_TEST2
#ifdef RENDER_TEST
#include <nanovg.h>
#include "color_util.h"
#include "seq_math.h"
#include "logging.h"

int mainTest(void (*drawFn)(NVGcontext*,int,int,float));
void testAAScissorBleeding(NVGcontext* ctx, int winW, int winH, float pxratio) {
	nvgBeginFrame(ctx, winW, winH, pxratio);
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
int mainHost();
int main(int argc, char* argv[]) {
	return mainHost();
}
#endif
