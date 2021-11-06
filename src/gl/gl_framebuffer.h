#pragma once
#include <array>
#include "glheaders.h"
#include "math/seq_math.h"
#include "math/vec.h"
#include "gl_util.h"
#include "assert_dbg.h"

#define MAX_COLOR_ATT 8
#define GL_ERROR_CHECKS true
class FrameBuffer {
public:
	static int frambuffersRefCount;
	static FrameBuffer* lastBound;
	static void unbindFramebuffer() {
		lastBound = nullptr;
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		if (GL_ERROR_CHECKS)
			checkGLError("FrameBuffers.glUnbindCurrentFrameBuffer");
	}
	static void unbindReadFramebuffer() {
		lastBound = nullptr;
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		if (GL_ERROR_CHECKS)
			checkGLError("FrameBuffers.glUnbindCurrentReadBuffer");
	}

	const int renderWidth;
	const int renderHeight;
	GLuint fb = 0;
	bool isComplete = false;
	bool hasDepth = false;
	bool isShadowDepthBuffer = false;
	bool hasCustomClearColor = false;
	int numColorTextures = 0;
	int highestColorAtt = 0;
	int depthTexture = 0;
	std::array<vec4, MAX_COLOR_ATT> clearColor;
	std::array<GLuint, MAX_COLOR_ATT> colorAttTextures;
	std::array<int, MAX_COLOR_ATT> colorAttFormats;
	std::array<int, MAX_COLOR_ATT> colorAttMinFilters;
	std::array<int, MAX_COLOR_ATT> colorAttMagFilters;
	int colorTexExtFmt = GL_BGRA;
	int colorTexExtType = GL_UNSIGNED_INT_8_8_8_8_REV;
	int textureType = GL_TEXTURE_2D;
	int depthFmt = GL_DEPTH_COMPONENT32;
	int mipmapLevels = 0;
	int anisotropicFilterLevel = -1;
#define CLEAR(xarr, v) \
	for (int i = 0; i < MAX_COLOR_ATT; i++) xarr[i] = v;

	FrameBuffer(int w, int h, int type = GL_RGBA16F, bool depthBuffer = true, vec4 clrCol = vec4(0.0f)) :
		renderWidth(w), renderHeight(h) {
		CLEAR(clearColor, vec4(0.0f));
		CLEAR(colorAttTextures, 0);
		CLEAR(colorAttFormats, 0);
		CLEAR(colorAttMinFilters, 0);
		CLEAR(colorAttMagFilters, 0);
        setColorAtt(GL_COLOR_ATTACHMENT0, type);
        setFilter(GL_COLOR_ATTACHMENT0, GL_LINEAR, GL_LINEAR);
        setClearColor(GL_COLOR_ATTACHMENT0, clrCol);
//		depthBuffer = false;
        if (depthBuffer)
            setHasDepthAttachment();
	}
	FrameBuffer() : FrameBuffer(0, 0) {
	}
	~FrameBuffer() {
		if (isGLContextPresent())
			destroy();
	}
	void setColorTexExtFmt(int _colorTexExtFmt) {
		colorTexExtFmt = _colorTexExtFmt;
	}
	void setColorTexExtType(int _colorTexExtType) {
		colorTexExtType = _colorTexExtType;
	}
	void setTextureType(int _textureType) {
		textureType = _textureType;
	}
	void setMipmapLevels(int _mipmapLevels) {
		mipmapLevels = _mipmapLevels;
	}
	void setAnisotropicFilterLevel(int _anisotropicFilterLevel) {
		anisotropicFilterLevel = _anisotropicFilterLevel;
	}
	void setDepthFmt(int _depthFmt) {
		depthFmt = _depthFmt;
	}
	void setHasDepthAttachment() {
		hasDepth = true;

		//if clipcontrol is not supported we need this depth buffer format to get glDepthRangedNV working
//		if (Engine.INVERSE_Z_BUFFER && GL.isNVDepthBufferFloatSupported() && !GL.isClipControlSupported()) {
//			depthFmt = /*NVDepthBufferFloat*/GL_DEPTH_COMPONENT32F_NV;
//		}
	}

     void setShadowBuffer() {
        hasDepth = true;
        isShadowDepthBuffer = true;
    }

     void setup() {
        frambuffersRefCount++;
        int numTextures = 0;
        for (int i = 0; i < MAX_COLOR_ATT; i++) {
            if (colorAttFormats[i] != 0) {
                numTextures++;
            }
        }
        numColorTextures = numTextures;
        if (hasDepth) {
            numTextures++;
        }
        dbgassert(numTextures > 0 && "No textures defined");

        std::vector<GLuint> colorTextures(numTextures);

        glGenTextures(colorTextures.size(), colorTextures.data());
        if (GL_ERROR_CHECKS) checkGLError("FrameBuffers.glGenTextures");
        glGenFramebuffers(1, &fb);
        if (GL_ERROR_CHECKS) checkGLError("FrameBuffers.glGenFramebuffers");
        bind();
        if (GL_ERROR_CHECKS) checkGLError("FrameBuffers.glBindFramebuffer");
        GLenum glDrawNone = GL_NONE;
        glDrawBuffers(1, &glDrawNone);
        if (GL_ERROR_CHECKS) checkGLError("FrameBuffers.glDrawBuffers");
        glReadBuffer(GL_NONE);
        if (GL_ERROR_CHECKS) checkGLError("FrameBuffers.glReadBuffer");


        for (int i = 0; i < MAX_COLOR_ATT; i++) {
            highestColorAtt = math::max(i, highestColorAtt);
            if (highestColorAtt < i)
                highestColorAtt = i;
//            if (!end) {
//                end = colorAttFormats[i] == 0;
//            } else if (colorAttFormats[i] != 0) {
//                throw new GameError("Attachments must be in adjacent order");
//            }
            if (colorAttFormats[i] != 0) {
                int att = GL_COLOR_ATTACHMENT0 + i;
                GLuint tex = colorTextures[i];
                setupTexture(tex, colorAttFormats[i], colorAttMinFilters[i], colorAttMagFilters[i]);
                colorAttTextures[i] = tex;
                if (textureType == GL_TEXTURE_CUBE_MAP) {
                    glFramebufferTexture2D(GL_FRAMEBUFFER, att, GL_TEXTURE_CUBE_MAP_POSITIVE_X+0, tex, 0);
                } else {
                    glFramebufferTexture(GL_FRAMEBUFFER, att, tex, 0);
                }
                if (GL_ERROR_CHECKS) checkGLError("FrameBuffers.glFramebufferTexture");
            }
        }

        if (hasDepth) {
            depthTexture = colorTextures.back();
            createDepthTextureAttachment(depthTexture);
        }
        int status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
        	isComplete = false;
        	printf("Framebuffer is incomplete (%d)\n", status);
        	return;
        }
        if (GL_ERROR_CHECKS) checkGLError("FrameBuffer glCheckFramebufferStatus");
        setDrawAll();
        if (GL_ERROR_CHECKS) checkGLError("FrameBuffer glDrawBuffers all");
        clearFrameBuffer();
        unbindFramebuffer();
        if (GL_ERROR_CHECKS) checkGLError("glBindFramebuffer 0");
    	isComplete = true;
    }
     void bindCubeMapFace(int i) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X+i, colorAttTextures[0], 0);
    }

     void bindAndClear() {
        bind();
        clearFrameBuffer();
    }
     void bind() {
        lastBound = this;
        glBindFramebuffer(GL_FRAMEBUFFER, fb);
        if (GL_ERROR_CHECKS) checkGLError("FrameBuffers.glBindFramebuffer");
//        Engine.setViewport(0, 0, getWidth(), getHeight());
    }
     void bindRead() {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fb);
        if (GL_ERROR_CHECKS) checkGLError("FrameBuffers.glBindFramebuffer");
    }

     void setupTexture(int texture, int format, int minfilter, int magFilter) {
        glBindTexture(textureType, texture);
        if (GL_ERROR_CHECKS) checkGLError("FrameBuffers.glBindTexture");
        glTexParameteri(textureType, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(textureType, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(textureType, GL_TEXTURE_MIN_FILTER, minfilter);
        glTexParameteri(textureType, GL_TEXTURE_MAG_FILTER, magFilter);
        glTexParameteri(textureType, GL_TEXTURE_MAX_LEVEL, mipmapLevels);
        if (textureType == GL_TEXTURE_CUBE_MAP) {
            glTexParameteri(textureType, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            if (GL_ERROR_CHECKS) checkGLError("glTexParameteri GL_TEXTURE_WRAP_R");
            for (int i = 0; i < 6; i++) {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, renderWidth, renderHeight, 0, GL_RGBA, GL_UNSIGNED_INT, nullptr);
                if (GL_ERROR_CHECKS) checkGLError("glTexImage2D GL13.GL_TEXTURE_CUBE_MAP_n");
            }
        } else if (format == GL_RGBA16UI) {
            glTexImage2D(textureType, 0, format, renderWidth, renderHeight, 0, GL_BGRA_INTEGER, GL_UNSIGNED_INT, nullptr);
        } else if (format == GL_RGB16UI) {
            glTexImage2D(textureType, 0, format, renderWidth, renderHeight, 0, GL_BGR_INTEGER, GL_UNSIGNED_INT, nullptr);
        } else {
            glTexImage2D(textureType, 0, format, renderWidth, renderHeight, 0, colorTexExtFmt, colorTexExtType, nullptr);
        }
        if (anisotropicFilterLevel > 0) {

            float f;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &f);
            if (anisotropicFilterLevel < f) {
                f = anisotropicFilterLevel;
            }
            if (f > 0) {
                glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_MAX_TEXTURE_MAX_ANISOTROPY, f);
            }
        }

        if (GL_ERROR_CHECKS) checkGLError("FrameBuffers.glTexImage2D");
        glBindTexture(textureType, 0);
    }

     void createDepthTextureAttachment(int texture) {
        glBindTexture(GL_TEXTURE_2D, texture);
        if (GL_ERROR_CHECKS) checkGLError("FrameBuffers.glBindTexture (depth)");
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        if (isShadowDepthBuffer) {

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//            glTexParameteri(GL_TEXTURE_2D, GL14.GL_TEXTURE_COMPARE_MODE, GL30.GL_COMPARE_REF_TO_TEXTURE);
        } else {

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//            glTexParameteri(GL_TEXTURE_2D, GL14.GL_DEPTH_TEXTURE_MODE, GL_LUMINANCE);//TODO: not working wiht core profile
        }
        if (GL_ERROR_CHECKS) checkGLError("FrameBuffers.glTexParameteri (depth)");

//        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
//        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
//        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
//        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        glTexImage2D(GL_TEXTURE_2D, 0, depthFmt, renderWidth, renderHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

//        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, renderWidth, renderHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        if (GL_ERROR_CHECKS) checkGLError("FrameBuffers.glTexImage2D (depth)");
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texture, 0);
        if (GL_ERROR_CHECKS) checkGLError("FrameBuffers.glFramebufferTexture (depth)");
        glBindTexture(GL_TEXTURE_2D, 0);
    }

	void setColorAtt(int att, int fmt) {
		att -= GL_COLOR_ATTACHMENT0;
		dbgassert(colorAttFormats[att] == 0 && "GL_COLOR_ATTACHMENT already set");
		colorAttFormats[att] = fmt;
		colorAttMinFilters[att] = GL_LINEAR;
		colorAttMagFilters[att] = GL_LINEAR;
	}

	void setFilter(int att, int filter, int magfilter) {
		att -= GL_COLOR_ATTACHMENT0;
		dbgassert(colorAttFormats[att] != 0 && "GL_COLOR_ATTACHMENT not set");
		colorAttMinFilters[att] = filter;
		colorAttMagFilters[att] = magfilter;
	}
	void setClearColor(int att, vec4 v) {
		att -= GL_COLOR_ATTACHMENT0;
		dbgassert(colorAttFormats[att] != 0 && "GL_COLOR_ATTACHMENT not set");
		clearColor[att] = v;
		hasCustomClearColor = att > 0;
	}
	GLuint detachColorTexture(int att) {
		dbgassert(colorAttFormats[att] != 0 && "GL_COLOR_ATTACHMENT not set");
        if (att == highestColorAtt) {
            highestColorAtt--;
        }
        numColorTextures--;
        GLuint t = colorAttTextures[att];
        colorAttTextures[att] = 0;
        return t;
    }
    void setDrawAll() {
    	setDrawMask(-1);
    }
    void setDrawMask(int mask) {
		dbgassert(lastBound == this && "trying to setDrawMask on unbound buffer");
		std::vector<GLenum> drawBufAtt;
		drawMask(drawBufAtt, mask);
		glDrawBuffers(drawBufAtt.size(), drawBufAtt.data());
    }
	void drawMask(std::vector<GLenum>& drawBufAtt, int mask) {
        int n = 0;
        for (int i = 0; i <= highestColorAtt; i++) {
            if (colorAttFormats[i] != 0 && (mask & (1<<i)) != 0) {
                int att = GL_COLOR_ATTACHMENT0 + i;
                drawBufAtt.push_back(att);
                n++;
            }
        }
        if (n == 0) {
            printf("drawbuffers GL_NONE!\n");
            drawBufAtt.push_back(GL_NONE);
            n++;
        }
    }
    void clearFrameBuffer() {
		dbgassert(lastBound == this && "trying to clearFrameBuffer unbound buffer");
        if (!hasCustomClearColor) {
            setDrawAll();
            int flags = GL_COLOR_BUFFER_BIT;
            if (hasDepth)
                flags |= GL_DEPTH_BUFFER_BIT;
            glClearColor(clearColor[0][0], clearColor[0][1], clearColor[0][2], clearColor[0][3]);
            glClear(flags);
        } else {
            glClear(GL_DEPTH_BUFFER_BIT);
            for (int i = 0; i <= highestColorAtt; i++) {
                if (colorAttFormats[i] != 0) {
                	GLenum att = GL_COLOR_ATTACHMENT0 + i;
                    glDrawBuffers(1, &att);
                    glClearColor(clearColor[i][0], clearColor[i][1], clearColor[i][2], clearColor[i][3]);
                    glClear(GL_COLOR_BUFFER_BIT);
                }
            }
            setDrawAll();
        }
        if (GL_ERROR_CHECKS) checkGLError("clearFrameBuffer");
    }
	void clearColorBuffer() {
		dbgassert(lastBound == this && "trying to clear unbound buffer");
		setDrawMask(1);
		glClear(GL_COLOR_BUFFER_BIT);
		setDrawMask(-1);
	}

	void clearColorBufferBlack() {
		dbgassert(lastBound == this && "trying to clear unbound buffer");
		setDrawMask(1);
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT);
		vec4& ccv = clearColor[0];
		glClearColor(ccv.x, ccv.y, ccv.z, ccv.w);
		setDrawMask(-1);
	}

    void clearDepth() {
		dbgassert(lastBound == this && "trying to clearFrameBuffer unbound buffer");
        if (hasDepth) {
            glClear(GL_DEPTH_BUFFER_BIT);

            if (GL_ERROR_CHECKS)
                checkGLError("clearFrameBuffer Depth");
        }
    }

    void destroy() {
        if (fb != 0) {
            glDeleteFramebuffers(1, &fb);
            fb = 0;
            if (GL_ERROR_CHECKS) checkGLError("FrameBuffers.glDeleteFramebuffers");
            std::vector<GLuint> colorTextures;
            for (int i = 0; i < MAX_COLOR_ATT; i++) {
                if (colorAttTextures[i] != 0) {
                    colorTextures.push_back(colorAttTextures[i]);
                }
            }
            if (hasDepth) {
                colorTextures.push_back(depthTexture);
            }
            glDeleteTextures(colorTextures.size(), colorTextures.data());
            if (GL_ERROR_CHECKS) checkGLError("FrameBuffers.glDeleteTextures");
            frambuffersRefCount--;
        }
    }

};
