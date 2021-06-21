#ifdef NANOVG_GL3
	uniform vec2 viewSize;
	in vec2 vertex;
	in vec2 tcoord;
	out vec2 ftcoord;
	out vec2 fpos;
#else
	uniform vec2 viewSize;
	attribute vec2 vertex;
	attribute vec2 tcoord;
	varying vec2 ftcoord;
	varying vec2 fpos;
#endif
uniform vec4 renderInfo;
void main(void) {
	ftcoord = tcoord;
	vec2 vPos = vertex;
	// vPos.x += sin(renderInfo.x*1.0*3.14*2.0)*32.0;
	fpos = vPos;
	gl_Position = vec4(2.0*vPos.x/viewSize.x - 1.0, 1.0 - 2.0*vPos.y/viewSize.y, 0, 1);
}