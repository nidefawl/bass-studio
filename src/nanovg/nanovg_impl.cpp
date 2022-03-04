#include "glheaders.h"
#include "nanovg_internal.h"
#include "nanovg_gl.h"

#include "platform.h"
#include "hires_timer.h"
#include <algorithm>
#include <glm/gtc/type_ptr.hpp>

hires_timer_t timer;
extern "C" {
void resetShaderTimeOffset() {
	timer.reset();
}
float glnvg__getTimeMillisf() {
	return (float) timer.getTimeDouble() * 1000.0;
}
}

#ifdef NVG_3D_MODE
#include <math/vec.h>
#include <math/mat.h>
#include <math/seq_math.h>
#include "logging.h"

void glnvg__updateMvpCXX(int u_loc_mvp, float w, float h) {
	float fTime = glnvg__getTimeMillisf() * 0.1f;
	float f		= sin(fTime * 0.001f);
	float f2	= sin(fTime * 0.004f);

	float fovYDeg		= 90.0f;//(f*0.5f+0.5f) * 120+30;
	float fovYRads		= fovYDeg * M_PI / 180.0f;
	float aspectRatio	= (float) (w / (float) h);
	float halfSizeX		= w / 2.0f;
	float halfSizeY		= h / 2.0f;
	float itemSizeRatio	= halfSizeY / halfSizeX;
	float tanfR			= tanf(fovYRads / 2.0f);
	float distanceVertical	 = halfSizeY / tanfR;
	float distanceHorizontal = distanceVertical * itemSizeRatio / aspectRatio;
	float cameraDistance	 = math::max(distanceVertical, distanceHorizontal);

	mat4x4 matProj;
	mat4x4 matView;
	mat4x4 matModel;
	matProj = glm::perspective(fovYRads, aspectRatio, 0.05f, 2000.0f);
	matView = glm::lookAt(
			glm::vec3(w / 2.0f, h / 2.0f, -cameraDistance * 1.5f),
			glm::vec3(w / 2.0f, h / 2.0f, 0.0),
			glm::vec3(0.0, -1.0, 0.0));

	float angleRad  = (f * 15) * M_PI / 180.0f;
	float angleRad2 = (f2 * 10) * M_PI / 180.0f;

	matModel = glm::translate(glm::identity<glm::mat4>(), glm::vec3(w / 2.0f, h / 2.0f, 0.0f));
	matModel = glm::rotate(matModel, angleRad, glm::vec3(0.0, 0.0, 1.0));
	matModel = glm::rotate(matModel, angleRad2, glm::vec3(1.0, 0.0, 0.0));
	matModel = glm::translate(matModel, glm::vec3(-w / 2.0f, -h / 2.0f, 0.0f));

	glm::mat4 matViewProj = matProj * matView * matModel;
	glUniformMatrix4fv(u_loc_mvp, 1, GL_FALSE, value_ptr(matViewProj));
}
#endif

extern "C" {
void glnvg__updateMvp(int u_loc_mvp, float w, float h) {
#ifdef NVG_3D_MODE
	glnvg__updateMvpCXX(u_loc_mvp, w, h);
#endif
}
}
