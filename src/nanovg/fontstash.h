//
// Copyright (c) 2009-2013 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#ifndef FONS_H
#define FONS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "assert_dbg.h"
#include <stddef.h>

void* fons__tmpalloc(size_t size, void* up);
void fons__tmpfree(void* ptr, void* up);
#define STBTT_malloc(x,u)    fons__tmpalloc(x,u)
#define STBTT_free(x,u)      fons__tmpfree(x,u)

#define FONS_NOTUSED(v)  (void)sizeof(v)

#ifdef FONS_USE_FREETYPE

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ADVANCES_H
#include <math.h>

struct FONSttFontImpl {
    FT_Face font;
};
typedef struct FONSttFontImpl FONSttFontImpl;

#else

#include <stb/stb_truetype.h>

struct FONSttFontImpl {
    stbtt_fontinfo font;
};
typedef struct FONSttFontImpl FONSttFontImpl;

#endif

#ifndef FONS_SCRATCH_BUF_SIZE
#	define FONS_SCRATCH_BUF_SIZE 96000
#endif
#ifndef FONS_HASH_LUT_SIZE
#	define FONS_HASH_LUT_SIZE 256
#endif
#ifndef FONS_INIT_FONTS
#	define FONS_INIT_FONTS 4
#endif
#ifndef FONS_INIT_GLYPHS
#	define FONS_INIT_GLYPHS 256
#endif
#ifndef FONS_INIT_ATLAS_NODES
#	define FONS_INIT_ATLAS_NODES 256
#endif
#ifndef FONS_VERTEX_COUNT
#	define FONS_VERTEX_COUNT 1024
#endif
#ifndef FONS_MAX_STATES
#	define FONS_MAX_STATES 20
#endif
#ifndef FONS_MAX_FALLBACKS
#	define FONS_MAX_FALLBACKS 20
#endif
#define FONS_INVALID -1


enum FONSflags {
    FONS_ZERO_TOPLEFT    = 1,
    FONS_ZERO_BOTTOMLEFT = 2,
    FONS_KERNING_ADVANCE = 4,
};

enum FONSalign {
    // Horizontal align
    FONS_ALIGN_LEFT 	= 1<<0,	// Default
    FONS_ALIGN_CENTER 	= 1<<1,
    FONS_ALIGN_RIGHT 	= 1<<2,
    // Vertical align
    FONS_ALIGN_TOP 		= 1<<3,
    FONS_ALIGN_MIDDLE	= 1<<4,
    FONS_ALIGN_BOTTOM	= 1<<5,
    FONS_ALIGN_BASELINE	= 1<<6, // Default
    FONS_ALIGN_MIDDLE_INCLUDE_DESCENDER = 1<<7
};

enum FONSglyphBitmap {
    FONS_GLYPH_BITMAP_OPTIONAL = 1,
    FONS_GLYPH_BITMAP_REQUIRED = 2,
};

enum FONSerrorCode {
    // Font atlas is full.
    FONS_ATLAS_FULL = 1,
    // Scratch memory used to render glyphs is full, requested size reported in 'val', you may need to bump up FONS_SCRATCH_BUF_SIZE.
    FONS_SCRATCH_FULL = 2,
    // Calls to fonsPushState has created too large stack, if you need deep state stack bump up FONS_MAX_STATES.
    FONS_STATES_OVERFLOW = 3,
    // Trying to pop too many states fonsPopState().
    FONS_STATES_UNDERFLOW = 4,
};

struct FONSparams {
    int width, height;
    unsigned char flags;
    void* userPtr;
    int (*renderCreate)(void* uptr, int width, int height);
    int (*renderResize)(void* uptr, int width, int height);
    void (*renderUpdate)(void* uptr, int* rect, const unsigned char* data);
    void (*renderDraw)(void* uptr, const float* verts, const float* tcoords, const unsigned int* colors, int nverts);
    void (*renderDelete)(void* uptr);
};
typedef struct FONSparams FONSparams;

struct FONSquad
{
    float x0,y0,s0,t0;
    float x1,y1,s1,t1;
};
typedef struct FONSquad FONSquad;

struct FONStextIter {
    float x, y, nextx, nexty, scale, spacing;
    unsigned int codepoint;
    short isize, iblur;
    struct FONSfont* font;
    int prevGlyphIndex;
    const char* str;
    const char* next;
    const char* end;
    unsigned int utf8state;
    int bitmapOption;
};
typedef struct FONStextIter FONStextIter;

typedef struct FONScontext FONScontext;

// Constructor and destructor.
FONScontext* fonsCreateInternal(FONSparams* params);
void fonsDeleteInternal(FONScontext* s);

void fonsSetErrorCallback(FONScontext* s, void (*callback)(void* uptr, int error, int val), void* uptr);
// Returns current atlas size.
void fonsGetAtlasSize(FONScontext* s, int* width, int* height);
// Expands the atlas size.
int fonsExpandAtlas(FONScontext* s, int width, int height);
// Resets the whole stash.
int fonsResetAtlas(FONScontext* stash, int width, int height);
void fonsUnloadUnused(FONScontext* stash);

// Add fonts
int fonsAddFont(FONScontext* s, const char* name, const char* path, int fontIndex);
int fonsAddFontMem(FONScontext* s, const char* name, unsigned char* data, int ndata, int freeData, int fontIndex);
int fonsGetFontByName(FONScontext* s, const char* name);

// State handling
void fonsPushState(FONScontext* s);
void fonsPopState(FONScontext* s);
void fonsClearState(FONScontext* s);

// State setting
void fonsSetSize(FONScontext* s, float size);
void fonsSetColor(FONScontext* s, unsigned int color);
void fonsSetSpacing(FONScontext* s, float spacing);
void fonsSetBlur(FONScontext* s, float blur);
void fonsSetAlign(FONScontext* s, int align);
void fonsSetFont(FONScontext* s, int font);

// Draw text
float fonsDrawText(FONScontext* s, float x, float y, const char* string, const char* end);

// Measure text
float fonsTextBounds(FONScontext* s, float x, float y, const char* string, const char* end, float* bounds);
void fonsLineBounds(FONScontext* s, float y, float* miny, float* maxy);
void fonsVertMetrics(FONScontext* s, float* ascender, float* descender, float* lineh);

// Text iterator
int fonsTextIterInit(FONScontext* stash, FONStextIter* iter, float x, float y, const char* str, const char* end, int bitmapOption);
int fonsTextIterNext(FONScontext* stash, FONStextIter* iter, struct FONSquad* quad);

// Pull texture changes
const unsigned char* fonsGetTextureData(FONScontext* stash, int* width, int* height);
int fonsValidateTexture(FONScontext* s, int* dirty);

// Draws the stash texture for debugging
void fonsDrawDebug(FONScontext* s, float x, float y);

int fonsAddFallbackFont(FONScontext* stash, int base, int fallback);
void fonsResetFallbackFont(FONScontext* stash, int base);
void fonsSetSize(FONScontext* stash, float size);
void fonsSetColor(FONScontext* stash, unsigned int color);
void fonsSetSpacing(FONScontext* stash, float spacing);
void fonsSetBlur(FONScontext* stash, float blur);
void fonsSetAlign(FONScontext* stash, int align);
void fonsSetFont(FONScontext* stash, int font);
void fonsPushState(FONScontext* stash);
void fonsPopState(FONScontext* stash);
void fonsClearState(FONScontext* stash);

struct FONSglyph
{
    unsigned int codepoint;
    int index;
    int next;
    short size, blur;
    short x0,y0,x1,y1;
    short xadv,xoff,yoff;
    short framesRendered;
};
typedef struct FONSglyph FONSglyph;

struct FONSfont
{
    FONSttFontImpl font;
    char name[64];
    unsigned char* data;
    int dataSize;
    unsigned char freeData;
    float ascender;
    float descender;
    float lineh;
    FONSglyph* glyphs;
    int cglyphs;
    int nglyphs;
    int lut[FONS_HASH_LUT_SIZE];
    int fallbacks[FONS_MAX_FALLBACKS];
    int nfallbacks;
};
typedef struct FONSfont FONSfont;

struct FONSstate
{
    int font;
    int align;
    float size;
    unsigned int color;
    float blur;
    float spacing;
};
typedef struct FONSstate FONSstate;

struct FONSatlasNode {
    short x, y, width;
};
typedef struct FONSatlasNode FONSatlasNode;

struct FONSatlas
{
    int width, height;
    FONSatlasNode* nodes;
    int nnodes;
    int cnodes;
};
typedef struct FONSatlas FONSatlas;

struct FONScontext
{
    FONSparams params;
    float itw,ith;
    unsigned char* texData;
    int dirtyRect[4];
    FONSfont** fonts;
    FONSatlas* atlas;
    int cfonts;
    int nfonts;
    float verts[FONS_VERTEX_COUNT*2];
    float tcoords[FONS_VERTEX_COUNT*2];
    unsigned int colors[FONS_VERTEX_COUNT];
    int nverts;
    unsigned char* scratch;
    int nscratch;
    FONSstate states[FONS_MAX_STATES];
    int nstates;
    void (*handleError)(void* uptr, int error, int val);
    void* errorUptr;
#ifdef FONS_USE_FREETYPE
    FT_Library ftLibrary;
#endif
};

#endif // FONTSTASH_H

#ifdef __cplusplus
}
#endif
