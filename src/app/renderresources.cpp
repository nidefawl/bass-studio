#include <stdint.h>
#include <memory>
#include <vector>
#include <nanovg.h>

#include "str_util.h"
#include "fileio.h"
#include "renderresources.h"

#include <GLFW/glfw3.h>

#include <assert.h>


using ImgData = std::shared_ptr<uint8_t>;

namespace {
	void setColor(uint8_t* b, int i) {
		b[0] = i & 0xFF; i = i >> 8;
		b[1] = i & 0xFF; i = i >> 8;
		b[2] = i & 0xFF; i = i >> 8;
		b[3] = i & 0xFF; i = i >> 8;
	};
	ImgData createDashedLineTexture(int w) {
		int size = w*w*4;
		std::shared_ptr<uint8_t> imageData(new uint8_t[size], std::default_delete<uint8_t[]>());

		uint8_t *dataBuf = imageData.get();
		for (int x = 0; x < w; x++) {
			for (int y = 0; y < w; y++) {
				int idx = (x*w+y) * 4;
				int scale = 16;
				int ix = x % scale;
				setColor(dataBuf + idx, ix < 10 ? 0xffffffff : 0x00ffffff);
			}

		}
		return imageData;
	}
}

namespace RenderResources {
	NvgImageTexture imgDashedLine;
	NvgImageTexture imgIcons[NUM_IMGS];
	namespace {
		void load(NVGcontext* vg, String path, ImageBuf& out) {
			if (ReadImage(path, out) < 0) {
				my_printf("Error loading image %s\n", StringAsCStr(path));
			} else {
				my_printf("%s loaded: %dx%d %d-channel, bufsize: %d\n", StringAsCStr(path), out.w, out.h, out.bitdepth, out.bytes.size());
			}
		}
	}
	void initResources(NVGcontext* vg) {
		{
			ImageBuf imgIconsBuf[NUM_IMGS];
			for (int i = 0; i < NUM_IMGS; i++) {
				ImageBuf& buf = imgIconsBuf[i];
				assert((int)buf.bytes.size() == buf.w*buf.h * 4);
			}
			load(vg, StringFormat("res/icons/synth_32px.png"), imgIconsBuf[ICON_SYNTH]);
			load(vg, StringFormat("res/icons/effect.png"), imgIconsBuf[ICON_EFFECT]);
			load(vg, StringFormat("res/icons/folder.png"), imgIconsBuf[ICON_FOLDER]);
			load(vg, StringFormat("res/icons/folder_open.png"), imgIconsBuf[ICON_FOLDER_OPEN]);
			load(vg, StringFormat("res/icons/file.png"), imgIconsBuf[ICON_FILE]);
			load(vg, StringFormat("res/icons/save.png"), imgIconsBuf[ICON_SAVE]);
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
			for (int i = 0; i < NUM_IMGS; i++) {
				ImageBuf& buf = imgIconsBuf[i];
				if (buf.w*buf.h == 0) {
					continue;
				}
				assert((int)buf.bytes.size() == buf.w*buf.h * 4);
				NvgImageTexture& nvgTex = imgIcons[i];
				int32_t nvgid = nvgCreateImageRGBA(vg, buf.w, buf.h, NVG_IMAGE_GENERATE_MIPMAPS, (const unsigned char*)buf.bytes.data());
				nvgImageSize(vg, nvgid, &nvgTex.width, &nvgTex.height);
				nvgTex.perContextId[vg] = nvgid;
			}
		}
		{
			int texSize = 64;
			ImgData dataB = createDashedLineTexture(texSize);
			int32_t nvgid = nvgCreateImageRGBA(vg, texSize, texSize, NVG_IMAGE_REPEATX | NVG_IMAGE_REPEATY | NVG_IMAGE_NEAREST, (const unsigned char*)dataB.get());
			nvgImageSize(vg, nvgid, &imgDashedLine.width, &imgDashedLine.height);
			imgDashedLine.perContextId[vg] = nvgid;
		}
	}

}
