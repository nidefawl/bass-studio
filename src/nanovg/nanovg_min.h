#ifndef NANOVG_MIN_H
#define NANOVG_MIN_H
#ifdef __cplusplus
extern "C" {
#endif
typedef struct NVGcontext NVGcontext;
typedef struct NVGcolor NVGcolor;
struct NVGcolor {
	float r,g,b,a;
};
NVGcolor nvgHSL(float h, float s, float l);
#ifdef __cplusplus
}
#endif
#endif
