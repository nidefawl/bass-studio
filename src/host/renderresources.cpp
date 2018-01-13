#include "renderresources.h"
#include "str_util.h"
#include "fileio.h"
#include <stdint.h>
#include <memory>
#include <nanovg.h>

#include <string>
#include <GLFW/glfw3.h>
#include <vector>
#include "fileio.h"
#include "mouse.h"


using ImgData = std::shared_ptr<uint8_t>;


namespace {
	ImgData createDashedLineTexture(int w) {
		int size = w*w*4;
		union byteint
		{
			uint8_t b[4];
			uint32_t i;
		};
		std::shared_ptr<uint8_t> imageData(new uint8_t[size], std::default_delete<uint8_t[]>());

		for (int x = 0; x < w; x++) {
			for (int y = 0; y < w; y++) {
				int idx = (x*w+y) * 4;
				byteint rgb;
				{

					int scale = 8;
					int ix = x%scale;
					int iy = y%scale;
					rgb.i = 0x00ffffff;
					if (ix == iy) {
						rgb.i = -1;
					}
				}
				int scale = 16;
				int ix = x%scale;
				rgb.i = 0x00ffffff;
				if (ix<10) {
					rgb.i = 0xffffffff;
				}
				uint8_t *dataBuf = imageData.get();
				for (int i = 0; i < 4; i++)
					dataBuf[idx+i] = rgb.b[i];
			}

		}
		return imageData;
	}
}

namespace RenderResources {
	NvgImageTexture imgDashedLine;
	NvgImageTexture imgIcons[NUM_IMGS];
	GLFWcursor* cursors[NUM_CURSORS];
	void load(NVGcontext* vg, String path, ImageBuf& out) {
		if (ReadImage(path, out) < 0) {
			my_printf("Error loading image %s\n", StringAsCStr(path));
		} else {
			my_printf("%s loaded: %dx%d %d-channel, bufsize: %d\n", StringAsCStr(path), out.w, out.h, out.bitdepth, out.bytes.size());
		}
	}
	void init(GLFWwindow *glfw, NVGcontext* vg) {
		int texSize = 64;
		{
			ImgData data = createDashedLineTexture(texSize);
			imgDashedLine.id = nvgCreateImageRGBA(vg, texSize, texSize, NVG_IMAGE_REPEATX|NVG_IMAGE_REPEATY|NVG_IMAGE_NEAREST, (const unsigned char*)data.get());
			nvgImageSize(vg, imgDashedLine.id, &imgDashedLine.width, &imgDashedLine.height);
		}
		{
			ImageBuf imgIconsBuf[NUM_IMGS];
			ImageBuf imgCursors[NUM_CURSORS];
			load(vg, StringFormat("res/icons/synth_32px.png"), imgIconsBuf[ICON_SYNTH]);
			load(vg, StringFormat("res/icons/effect.png"), imgIconsBuf[ICON_EFFECT]);
			load(vg, StringFormat("res/icons/folder.png"), imgIconsBuf[ICON_FOLDER]);
			load(vg, StringFormat("res/icons/folder_open.png"), imgIconsBuf[ICON_FOLDER_OPEN]);
			load(vg, StringFormat("res/icons/file.png"), imgIconsBuf[ICON_FILE]);
			load(vg, StringFormat("res/icons/copy.png"), imgIconsBuf[ICON_COPY]);
			load(vg, StringFormat("res/icons/paste.png"), imgIconsBuf[ICON_PASTE]);
			load(vg, StringFormat("res/icons/cut.png"), imgIconsBuf[ICON_CUT]);
			load(vg, StringFormat("res/icons/adjust.png"), imgIconsBuf[ICON_ADJUST]);
			load(vg, StringFormat("res/icons/close.png"), imgIconsBuf[ICON_CLOSE]);
			load(vg, StringFormat("res/icons/bypass.png"), imgIconsBuf[ICON_BYPASS]);
			load(vg, StringFormat("res/icons/loop.png"), imgIconsBuf[ICON_LOOP]);
			load(vg, StringFormat("res/icons/arr_down.png"), imgIconsBuf[ICON_ARR_DOWN]);
			load(vg, StringFormat("res/icons/arr_left.png"), imgIconsBuf[ICON_ARR_LEFT]);
			load(vg, StringFormat("res/icons/arr_right.png"), imgIconsBuf[ICON_ARR_RIGHT]);
			load(vg, StringFormat("res/icons/plus.png"), imgIconsBuf[ICON_PLUS]);
			load(vg, StringFormat("res/icons/minus.png"), imgIconsBuf[ICON_MINUS]);
			load(vg, StringFormat("res/icons/automation.png"), imgIconsBuf[ICON_AUTOMATION]);
			load(vg, StringFormat("res/led.png"), imgIconsBuf[IMG_LED]);
			load(vg, StringFormat("res/led_off.png"), imgIconsBuf[IMG_LED_OFF]);
			load(vg, StringFormat("res/led_glow.png"), imgIconsBuf[IMG_LED_GLOW]);
			for (int i = 0; i < 6; i++) {
				load(vg, StringFormat("res/cursors/cursor%02d.png", i), imgCursors[i]);
			}
			for (int i = 0; i <= NUM_IMGS; i++) {
				ImageBuf& buf = imgIconsBuf[i];
				if (buf.w*buf.h == 0) {
					continue;
				}
				NvgImageTexture& nvgTex = imgIcons[i];
				nvgTex.id = nvgCreateImageRGBA(vg, buf.w, buf.h, NVG_IMAGE_GENERATE_MIPMAPS, (const unsigned char*)buf.bytes.data());
				nvgImageSize(vg, nvgTex.id, &nvgTex.width, &nvgTex.height);
			}
			cursors[0] = NULL;
			for (int i = 0; i < 6; i++) {
				ImageBuf& buf = imgCursors[i];
				if (buf.w*buf.h == 0) {
					continue;
				}
				GLFWimage image;
				image.width = buf.w;
				image.height = buf.h;
				image.pixels = &buf.bytes[0];
				int posx = image.width/2;
				int posy = image.height/2;
				if (i+1 == CURSOR_DUPLICATE) {
					posx = 0;
					posy = 0;
				}
				if (i+1 == CURSOR_CLIP_SIZE_LEFT) {
					posx = 4;
				}
				if (i+1 == CURSOR_CLIP_SIZE_RIGHT) {
					posx = 12;
				}
				cursors[i+1] = glfwCreateCursor(&image, posx, posy);
			}
		}
	}

}
