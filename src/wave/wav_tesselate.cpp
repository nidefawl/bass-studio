#define _USE_MATH_DEFINES
#include <math.h>
#include <vector>
#include <algorithm>

#include <glm/glm.hpp>

#include "audiosample.h"
#include "audiowaveform.h"
#include "logging.h"

using glm::mat4x4;
using glm::vec2;
using glm::vec3;
using glm::vec4;
using vec2list = std::vector<vec2>;

using samplechannel_t = std::vector<float>;
void renderWaveProcessed(audiosample_t* sample, float x, float y, audioclip_texture_t* waveformshape, SampleMethod method, std::vector<vec2list>& channels) {
	if (sample->nSamples) {
		int width = waveformshape->size.x;
		int height = waveformshape->size.y;
		int nMaxDowns = sample->downsampled.size()+1;
		int nLevel = 0;
		int scale = 1;
		audioclip_texture_t waveformScaled = *waveformshape;
		while ((waveformScaled.res > 64) && nLevel+1 < nMaxDowns) {
			waveformScaled.res /= 2.0;
			nLevel++;
			scale <<= 1;
			break;
		}
		waveformScaled.sampleBeginOffset /= scale;
		waveformScaled.sampleBegin /= scale;
		waveformScaled.sampleEnd /= scale;
		assert (nLevel == 0 || nLevel-1 < sample->downsampled.size());
		std::vector<samplechannel_t>& smpCh = nLevel == 0 ? sample->samples : sample->downsampled[nLevel-1];
		int upscale = 1;
		double dres = waveformScaled.res;
		while (dres >= 4.0 && upscale < 64) {
			dres /= 2.0;
			upscale*=2;
		}
		my_printf("upscale %d, waveformScaled.res %f\n", upscale, waveformScaled.res);
//		while (samplesPerPx*subsampling < 8 && subsampling*2 < 4) {
//			subsampling *= 2;
//		}
		double samplesPerPx = waveformScaled.res;
		double renderOffset = waveformScaled.sampleBeginOffset - waveformScaled.sampleBegin;
		int verticesPerPx = waveformScaled.quality;
		while (dres >= 4.0) {
			dres /= 2.0;
			verticesPerPx*=2;
		}
		float channelHeight = height / (float) sample->nChannels;
		float subsampleScale = 1.0f / (float) verticesPerPx;
		float samplesToPx = 1.0f/samplesPerPx;
		double vOffset = subsampleScale;
		int nVecs = width * verticesPerPx;
		float fsMin = 0.0f;
		float fsMax = 0.0f;
		int nvecs = 0;
		for (int iChannel = 0; iChannel < sample->nChannels; iChannel++) {
			vec2list vecs;
			if (nVecs > 0)
				vecs.reserve(nVecs);
			double samplePos = waveformScaled.sampleBeginOffset;
			float px = x;
			float py = y + channelHeight * iChannel + channelHeight / 2.0f;
			auto& samplesCh = smpCh[iChannel];
			float* samplesChPtr = samplesCh.data();
			size_t lenSamplesCh = samplesCh.size();
			switch (method) {
				case SampleMethod::sample_peakdetect:
				{
					int32_t sampleIdxStart = std::floor(samplePos-waveformScaled.sampleBegin);
					if (sampleIdxStart >= lenSamplesCh) {
						break;
					}
					assert(sampleIdxStart >= 0);
					float mx = samplesChPtr[sampleIdxStart];
					float mn = mx;
					int mx_pos = sampleIdxStart;
					int mn_pos = sampleIdxStart;
					bool findMax = false;
					float delta = 0.1f;
					float lastPtX = 0;
					float fY = -mx * channelHeight / 2.0f;
					vec2 vec { px, py + fY };
					vecs.push_back(std::move(vec));
					for (; samplePos < waveformScaled.sampleEnd; ) {
						double sampleOffset = samplePos-waveformScaled.sampleBegin;

						if (sampleOffset >= lenSamplesCh) { //TODO: no loop!
//							sampleOffset = std::max(0.0, sampleOffset-lenSamplesCh);
							break;
						}
						int32_t sampleIdx = std::floor(sampleOffset);
						float data = samplesChPtr[sampleIdx];
						int noffset = 0;
						int c = 0;
						int blurrange = 4;
						for (;noffset<blurrange; noffset++) {
							if (sampleIdx+noffset > 0 && sampleIdx + noffset < lenSamplesCh) {
								data+=samplesChPtr[sampleIdx+noffset];
								c++;
							}
							if (sampleIdx-noffset > 0 && sampleIdx - noffset < lenSamplesCh) {
								data+=samplesChPtr[sampleIdx-noffset];
								c++;
							}
						}
						if (c > 0) {
							data /= (float)c;
						}
						if ((int)sampleIdx%upscale==0) {
							float fCurX = (sampleOffset-renderOffset) * samplesToPx;
							if (fCurX > lastPtX+vOffset) {
								float fY = -data * channelHeight / 2.0f;
								vec2 vec { px + fCurX, py + fY };
								vecs.push_back(std::move(vec));
								if (fCurX >= width) {
									break;
								}
								lastPtX = fCurX;
								mn = data;
								mn_pos = sampleIdx;
								mx = data;
								mx_pos = sampleIdx;
							}

						}
						samplePos += 1;
					}
				}
					break;
				case SampleMethod::sample_minmax:
				{

					float lastPtX = 0;
					vec2 v0 { px, py };
		//			v0 *= G_SCALE;
					vecs.push_back(std::move(v0));
					float smin = 0;
					float smax = 0;
					bool minOrMax = false;
					for (; samplePos < waveformScaled.sampleEnd; samplePos+=1.0) {
						double sampleOffset = samplePos-waveformScaled.sampleBegin;
						if (sampleOffset >= lenSamplesCh) {
							break;
						}
						int32_t sampleIdx = std::floor(sampleOffset);
						float data = samplesChPtr[sampleIdx];
						smin = std::min(smin, data);
						smax = std::max(smax, data);
						float fCurX = (sampleOffset-renderOffset)*samplesToPx;
						if (fCurX - lastPtX >= subsampleScale) {
							float fs = minOrMax ? smin : smax;
							float fY = -fs * channelHeight / 2.0f;
							vec2 v1 { px + fCurX, py + fY };
		//					v1 *= G_SCALE;
							vecs.push_back(std::move(v1));
							fCurX += subsampleScale/2.0f;
							vec2 v2 { px + fCurX, py + 0 };
		//					v2 *= G_SCALE;
							vecs.push_back(std::move(v2));
							lastPtX = fCurX;
							smin = 0; smax = 0;
							minOrMax = !minOrMax;
							if (fCurX >= width) {
								break;
							}
						}
					}
				}
					break;
				case SampleMethod::sample_sum:
				{

					float lastPtX = 0;
					vec2 vec { px, py };
					vecs.push_back(std::move(vec));
					bool minOrMax = false;
					float sum = 0;
					int nCollect = 0;
					for (; samplePos < waveformScaled.sampleEnd; samplePos+=1.0) {
						double sampleOffset = samplePos-waveformScaled.sampleBegin;
						if (sampleOffset >= lenSamplesCh) {
							break;
						}
						int32_t sampleIdx = std::floor(sampleOffset);
						float data = samplesChPtr[sampleIdx];
						float fCurX = (sampleOffset-renderOffset)*samplesToPx;
					    float logScalef = log(std::abs(data)+1) / log(2.);
						sum += std::abs(logScalef);
						nCollect++;
						if (fCurX - lastPtX >= subsampleScale) {
							float f = 0;
							if (nCollect > 0) {
								float avg = sum / (float) nCollect;
								float fs = minOrMax ? -avg : avg;
								float fY = -fs * channelHeight / 2.0f;
								vec2 vec { px + fCurX, py + fY };
								vecs.push_back(std::move(vec));
								f += subsampleScale/2.0f;
								sum = 0;
								nCollect = 0;
							}
							vec2 vec2 { px + fCurX + f, py + 0 };
							vecs.push_back(std::move(vec2));
							if (fCurX+f >= width) {
								break;
							}
							lastPtX = fCurX;
							minOrMax = !minOrMax;
						}
					}
				}
					break;
				case SampleMethod::sample_minmax2:
				{

					float lastPtX = 0;
					vec2 v0 { px, py };
		//			v0 *= G_SCALE;
					vecs.push_back(std::move(v0));
					float smin = 0;
					float smax = 0;
					int32_t sampleIdxMin = 0;
					int32_t sampleIdxMax = 0;
					bool first = true;
					int32_t samplesColl = 0;
					float lastX = 0;
					for (; samplePos < waveformScaled.sampleEnd; samplePos+=1.0) {
						double sampleOffset = samplePos-waveformScaled.sampleBegin;
						if (sampleOffset >= lenSamplesCh) {
							break;
						}
						int32_t sampleIdx = std::floor(sampleOffset);
						float data = samplesChPtr[sampleIdx];
						if (smin > data || first) {
							smin = data;
							sampleIdxMin = sampleIdx;
						}
						if (smax < data || first) {
							smax = data;
							sampleIdxMax = sampleIdx;
						}
						float fCurX = (sampleOffset-renderOffset)*samplesToPx;
						if (samplesColl > 7) {
							assert (fCurX > lastX);
							lastX = fCurX;
							float fY1 = -smin * channelHeight / 2.0f;
							float fY2 = -smax * channelHeight / 2.0f;
							float f1 = sampleIdxMin < sampleIdxMax ? fY1 : fY2;
							float f2 = sampleIdxMin < sampleIdxMax ? fY2 : fY1;
							vec2 v1 { px + fCurX, py + f1 };
//							fCurX += subsampleScale/2.0f;
							fCurX += samplesToPx*4;
							vec2 v2 { px + fCurX, py + f2 };
							assert(v2.x>v1.x);
//							my_printf("v1.x %f\n", v1.x);
//							my_printf("v2.x %f\n", v2.x);
//
//							my_printf("fcurx %f, sampleoffset %f, renderoffset %f samplestopx %f\n", fCurX, sampleOffset, renderOffset, samplesToPx);
							assert(vecs.empty() || vecs.back().x < v1.x);
							vecs.push_back(std::move(v1));
							assert(vecs.empty() || vecs.back().x < v2.x);
							vecs.push_back(std::move(v2));
							lastPtX = fCurX;
							smin = 0; smax = 0;
							sampleIdxMin = 0;
							sampleIdxMax = 0;
							first = true;
							samplesColl = 0;
							if (fCurX >= width) {
								break;
							}
						}
						samplesColl++;
					}
				}
				break;
				case SampleMethod::sample_interp:
				default:
				{
					for (; samplePos < waveformScaled.sampleEnd; samplePos+=samplesPerPx*subsampleScale) {
						double sampleOffset = samplePos-waveformScaled.sampleBegin;
						if (sampleOffset >= lenSamplesCh) {
							break;
						}
						int32_t sampleIdx = std::floor(sampleOffset);
						float data = samplesChPtr[sampleIdx];
						fsMin = std::min(fsMin, data);
						fsMax = std::max(fsMax, data);
						float fCurX = (sampleOffset-renderOffset)*samplesToPx;
						float fY = -data * channelHeight / 2.0f;
						vec2 vec { px + fCurX, py + fY };
						vecs.push_back(std::move(vec));
						if (fCurX >= width) {
							break;
						}
					}
				}
				break;
			}
			nvecs += vecs.size();
//			if (vecs.size()) {
//				for (int i = 1; i < vecs.size(); i++) {
//					assert(vecs[i].x > vecs[i-1].x);
//				}
//				auto it = vecs.begin();
//				vec2 a = *it;
//				it++;
//				while (it != vecs.end()) {
//					vec2 b = *it;
//					float c = glm::distance(a, b);
//					if (c < 0.125f) {
//						vecs.erase(it);
////						my_printf("short dist: %f\n", c);
//						continue;
//					}
//					a = b;
//					it++;
//				}
//			}
			channels.push_back(std::move(vecs));
//			waveform.channels.push_back(std::move(vecs));
		}

//		my_printf("nvecs: %d\n", nvecs);
//		my_printf("min: %f, max: %f\n", fsMin, fsMax);
	}

}

