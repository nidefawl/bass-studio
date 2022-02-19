
#ifdef NANOVG_GL3
	uniform vec2 viewSize;
#ifdef NVG_3D_MODE
	in vec4 vertex;
#else
	in vec2 vertex;
#endif
	in vec2 tcoord;
	out vec2 ftcoord;
	out vec2 fpos;
#else
	uniform vec2 viewSize;
#ifdef NVG_3D_MODE
	attribute vec4 vertex;
#else
	attribute vec2 vertex;
#endif
	attribute vec2 tcoord;
	varying vec2 ftcoord;
	varying vec2 fpos;
#endif
uniform vec4 renderInfo;
#ifdef NVG_3D_MODE
uniform mat4 u_mvp;
#endif

void main(void) {
	ftcoord = tcoord;
	vec2 vPos = vertex.xy;
	float ft = sin(renderInfo.x);
	fpos = vPos;

#ifdef NVG_3D_MODE
	gl_Position = u_mvp * vertex;
#else
	gl_Position = vec4(2.0*vPos.x/viewSize.x - 1.0, 1.0 - 2.0*vPos.y/viewSize.y, 0, 1);
#endif
}