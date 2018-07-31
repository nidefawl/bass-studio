#include "glheaders.h"
#include <nanovg.h>
#include <nanovg_gl.h>
#include <nanovg_gl_utils.h>
#include <math.h>
// glm::translate, glm::rotate, glm::scale, glm::perspective
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include "drawwaveform.h"
#include "audiocache.h"
#include "platform.h"
#include "exceptions.h"
#include "logging.h"
#include "color_util.h"
#include "../host/mainctrl.h"


using glm::mat4x4;
using glm::ivec4;
using glm::ivec3;
using glm::ivec2;
using glm::vec3;
using glm::vec2;


bool checkGLError(const char* s);

namespace
{
	std::unique_ptr<waveformrender> g_instance;
}

void waveformrender::destroy() {
	g_instance.reset();
}
waveformrender* waveformrender::getInstance()
{
	return g_instance.get();
}
void waveformrender::setInstance(std::unique_ptr<waveformrender> host)
{
	g_instance = std::move(host);
}
void waveformrender::init() {
	renderer.init();
}
void waveformrender::getRenderedTextures(std::vector<TextureAtlas>& rendered) {
	for (auto& atlas : atlases) {
		if (atlas.entries.size()) {
			rendered.push_back(atlas);
		}
	}
}
void waveformrender::release(gui_waveform_texture_ref* waveformRef) {
	if (waveformRef->atlasId < 0) {
//		assert(0);
		return;
	}
	assert(waveformRef->atlasId >= 0 && waveformRef->atlasId < (int)atlases.size());
	auto& atlas = this->atlases[waveformRef->atlasId];
	assert(atlas.fb && atlas.glTexture > -1 && atlas.idx > -1);
	auto& vec = atlas.entries;
	const int id = waveformRef->atlasEntryId;
	auto it = std::find_if(vec.begin(), vec.end(), [id](const TextureAtlasEntry& entry) {
		return entry.id == id;
	});
	assert(it != vec.end());
	vec.erase(it);
	my_printf("erase entry %d from %d\n", id, waveformRef->atlasId);
	waveformRef->atlasId = -1;
	waveformRef->atlasEntryId = -1;
	waveformRef->rendered = false;
}
int waveformrender::queueUpdate(NVGcontext* ctxt, cachedaudio_t* audio, gui_waveform_texture_ref* waveformRef) {
	if (waveformRef->queued) {
		return 0;
	}
	waveform_update_task_t waveform_update_task{audio, waveformRef};
	queuedTasks.push_back(waveform_update_task);
	waveformRef->queued = true;
	return 1;
}
bool collides(ivec2 pos1, ivec2 size1, ivec2 pos2, ivec2 size2, ivec2& offset) {
	ivec2 rightBottom1 = pos1 + size1;
	ivec2 rightBottom2 = pos2 + size2;
	if (pos1.x >= rightBottom2.x) {
		return false;
	}
	if (rightBottom1.x <= pos2.x) {
		return false;
	}
	if (pos1.y >= rightBottom2.y) {
		return false;
	}
	if (rightBottom1.y <= pos2.y) {
		return false;
	}
	if (pos1.x >= pos2.x && pos1.x < rightBottom2.x) {
		offset.x += rightBottom2.x - pos1.x;
	}
	else if (pos1.y >= pos2.y && pos1.y < rightBottom2.y) {
		offset.y += rightBottom2.y - pos1.y;
	}
	return true;
}
struct _pos {
	ivec2 pos;
	ivec2 size;
};
bool anyCollision(std::vector<_pos>& positions, ivec2 pos1, ivec2 size1) {
	for(auto& entry : positions) {
		ivec2 offset(0, 0);
		if (collides(pos1, size1, entry.pos, entry.size, offset))
			return true;
	}
	return false;
}
bool waveformrender::findFreeSpot(ivec2 size, int& atlasIdx, ivec2& pos) {
	for (TextureAtlas& _atlas : atlases) {
		std::vector<_pos> positions;
		TextureAtlasEntry a;
		for(auto& entry : _atlas.entries) {
			positions.push_back(_pos{entry.pos, entry.size});
		}
		for(auto& entry : _atlas.queuedTasks) {
			positions.push_back(_pos{entry.waveformRef->pos, entry.waveformRef->size});
		}
		ivec2 tmpPos = {0, 0};
		while (1) {
			if (!anyCollision(positions, tmpPos, size)) {
				pos = tmpPos;
				atlasIdx = _atlas.idx;
				for(auto& entry : positions) {
					ivec2 offset(0, 0);
					bool b = collides(tmpPos, size, entry.pos, entry.size, offset);
					if (b) {
						assert(0);
					}
				}
				return true;
			}
			tmpPos.x+=16;
			if (tmpPos.x+size.x >= FBO_WIDTH) {
				tmpPos.x = 0;
				tmpPos.y+=16;
			}
			if (tmpPos.y+size.y >= FBO_HEIGHT) { // texture is filled
				break;
			}
		}
	}
	// create new texture
	TextureAtlas e;
	e.idx = atlases.size();
	atlases.push_back(e);

	pos = {0, 0};
	atlasIdx = e.idx;
	return true;
}

int waveformrender::renderUpdates(NVGcontext* ctxt, float pxRatio) {
	checkGLError("waveformrender::render start");

//	entry->inuse = true;
//	entry->props = *waveform;

	// Draw some stuff to an FBO as a test
	glViewport(0, 0, FBO_WIDTH, FBO_HEIGHT);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);

	checkGLError("fb prerender");


	//go over all waveforms that are queued up
	//assign them to free spots in framebuffertextures
	//go over all framebuffers: bind fb, go over all queue updates in that fb and tesselate + draw them
	for (waveform_update_task_t& waveformQueueEntry : this->queuedTasks) {

		gui_waveform_texture_ref* waveformRef = waveformQueueEntry.waveformRef;
		int atlasIdx = -1;
		ivec2 pos;
		vec2 v(waveformRef->waveform.size.x, waveformRef->waveform.size.y);
		v *= waveformRef->waveform.scale;
//		v.x *= 1.0f/waveformRef->waveform.scaleX;
		ivec2 sizeInternal = ivec2((int)std::ceil(v.x), (int)std::ceil(v.y));
//		assert(waveformRef->waveform.size.x <= 512);
		if (findFreeSpot(sizeInternal, atlasIdx, pos)) {

			TextureAtlas& _atlas = this->atlases[atlasIdx];
			_atlas.queuedTasks.push_back(waveformQueueEntry);
			waveformRef->pos = pos;
			waveformRef->size = sizeInternal;
			waveformRef->atlasId = atlasIdx;
			std::vector<_pos> positions;
			for(auto& entry : _atlas.entries) {
				positions.push_back(_pos{entry.pos, entry.size});
			}
			for(auto& entry : _atlas.queuedTasks) {
				positions.push_back(_pos{entry.waveformRef->pos, entry.waveformRef->size});
			}
			for(auto& entry : positions) {
				for(auto& entry2 : positions) {
					if (&entry2 == &entry) continue;
					ivec2 offset(0, 0);
					bool b = collides(entry2.pos, entry2.size, entry.pos, entry.size, offset);
					if (b) {
						my_printf("COLLISION\n",0);
					}
				}
			}
		}
	}
	this->queuedTasks.clear();
	//

	glUseProgram(renderer.program2dLines);
	for (TextureAtlas& _atlas : atlases) {
		bool clearFB = false;
		if (!_atlas.fb) {
			_atlas.fb = nvgluCreateFramebuffer(ctxt, FBO_WIDTH, FBO_HEIGHT, 0);
			if (!_atlas.fb) {
				throw new appexception("nvgluCreateFramebuffer error");
			}
			checkGLError("waveformrender::render nvgluCreateFramebuffer");
			_atlas.glTexture = nvgGetGLImageHandle(ctxt, _atlas.fb->image);
			clearFB = true;
		}
		if (_atlas.queuedTasks.empty()) {
			continue;
		}
		nvgluBindFramebuffer(_atlas.fb);
//		GLboolean isScissor=0;
//		glGetBooleanv(GL_SCISSOR_TEST, &isScissor);
//		assert(!isScissor);
		glClearColor(0, 0, 0, 0);
		if (clearFB) {
			glDisable(GL_SCISSOR_TEST);
			glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		}
		glEnable(GL_SCISSOR_TEST);
		for (waveform_update_task_t& waveformQueueEntry : _atlas.queuedTasks) {
			gui_waveform_texture_ref* waveformRef = waveformQueueEntry.waveformRef;
			cachedaudio_t* audio = waveformQueueEntry.audio;
			audioclip_texture_t& waveform = waveformRef->waveform;
			const int id = _atlas.nextIdx++;

			SampleMethod method = waveform.method;
			std::vector<std::vector<glm::vec2>> tesselatedWaveForms;
			tesselateWaveform(audio->sample.get(), 0, 0, &waveform, method, tesselatedWaveForms);
			Uniforms bakeOpt;
			bakeOpt.linecaps = vec2(LineCaps::none, LineCaps::none);
			bakeOpt.linejoin = waveform.linewidth > 1.75 ? LineJoin::round : LineJoin::miter;
			bakeOpt.miter_limit = 1.8f;
			bakeOpt.color = vec4(vec3(1), 1.0);
			//	uint32_t color = colorPalette[(nextIdx++%(COLOR_PALETTE_COLS-2))*COLOR_PALETTE_ROWS+3];
			//	bakeOpt.color = int32vec4(color);
			//	bakeOpt.color.w = 1.0;

			bakeOpt.linewidth = waveform.linewidth;
			bakeOpt.antialias = 1.0f;
			bakeOpt.scale = waveform.scale;
			renderer.bakePaths(tesselatedWaveForms, bakeOpt, this->bakedPath);
			mat4x4 matView = mat4x4(1.0);
			mat4x4 matModel = mat4x4(1.0);
			matModel[0][0] = waveform.scaleX;
			matView = glm::translate(matView, glm::vec3(waveformRef->pos.x, waveformRef->pos.y, 0));
			mat4x4 matProj = glm::ortho(0.f, (float) FBO_WIDTH, (float)FBO_HEIGHT, 0.f, 1.f, -1.f);
			glUniformMatrix4fv(renderer.u_projection, 1, GL_FALSE, value_ptr(matProj));
			glUniformMatrix4fv(renderer.u_view, 1, GL_FALSE, value_ptr(matView));
			glUniformMatrix4fv(renderer.u_model, 1, GL_FALSE, value_ptr(matModel));
			glUniform3f ( renderer.u_uniforms_shape, 1, bakedPath.numPaths*renderer.countUniforms, renderer.countUniforms);
			glBindTexture ( GL_TEXTURE_2D, bakedPath.uniforms_texture);
			glBindVertexArray ( bakedPath.vbo.vaoId );
			glBindBuffer ( GL_ELEMENT_ARRAY_BUFFER, bakedPath.vbo.vboIdxId);

			glScissor(waveformRef->pos.x, FBO_HEIGHT-waveformRef->pos.y-waveformRef->size.y, waveformRef->size.x, waveformRef->size.y);

			glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
			glDrawElements ( GL_TRIANGLES, bakedPath.vbo.nIndices, GL_UNSIGNED_INT, NULL);
			TextureAtlasEntry e;
			e.inuse = true;
			e.pos = waveformRef->pos;
			e.size = waveformRef->size;
			e.props = waveform;
			e.id = id;
			waveformRef->queued = false;
			waveformRef->rendered = true;
			waveformRef->atlasEntryId = id;
			_atlas.entries.push_back(e);
		}
		_atlas.queuedTasks.clear();

	}
	glClearColor(0, 0, 0, 0);
	glDisable(GL_SCISSOR_TEST);

	glBindVertexArray(0);
	nvgluBindFramebuffer(NULL);
	checkGLError("fb postrender");
	return 0;

}
void drawImage(NVGcontext* vg, int image, float alpha,
		float sx, float sy, float sw, float sh, // sprite location on texture
		float x, float y, float w, float h) // position and size of the sprite rectangle on screen
{
	float ax, ay;
	int iw,ih;
	NVGpaint img;

	nvgImageSize(vg, image, &iw, &ih);

	// Aspect ration of pixel in x an y dimensions. This allows us to scale
	// the sprite to fill the whole rectangle.
	ax = w / sw;
	ay = h / sh;

	img = nvgImagePattern(vg, x - sx*ax, y - sy*ay, (float)iw*ax, (float)ih*ay,
				0, image, alpha);
	nvgBeginPath(vg);
	nvgRect(vg, x,y, w,h);
	nvgFillPaint(vg, img);
	nvgFill(vg);
}

void waveformrender::draw(NVGcontext* ctxt, const gui_waveform_texture_ref* waveformRef, ivec2 size) {
	assert(waveformRef->atlasId >= 0 && waveformRef->atlasId < (int)atlases.size());
	auto& atlas = this->atlases[waveformRef->atlasId];
	assert(atlas.fb && atlas.glTexture > -1 && atlas.idx > -1);
	const audioclip_texture_t* waveImage = &waveformRef->waveform;
	drawImage(ctxt, atlas.fb->image, 1.0f, waveformRef->pos.x, waveformRef->pos.y, waveformRef->size.x, waveformRef->size.y, waveImage->startOffset.x, 0, waveImage->size.x /*-waveImage->startOffset.x */, waveImage->size.y);
//	for (auto& texture : textures) {
//		if (texture.idx == fbId) {
//			drawImage(ctxt, texture.fb->image, 1.0f, 0, 0, size.x*waveImage->scale, size.y*waveImage->scale, waveImage->startOffset.x, 0, size.x, size.y);
//			return;
//		}
//	}


}
