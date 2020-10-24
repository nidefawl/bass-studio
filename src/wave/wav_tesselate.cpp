#define _USE_MATH_DEFINES
#include <math.h>
#include <vector>
#include <algorithm>

#include "math/vec.h"
#include "math/mat.h"
#include "math/seq_math.h"
#include "audiosample.h"
#include "audiowaveform.h"
#include "logging.h"
#include "str_util.h"
#include "assert_dbg.h"

using vec2list = std::vector<vec2>;
//constexpr float MAX_RES = 512;
using samplechannel_t = std::vector<float>;
void tesselateWaveform(audiosample_t* sample, float x, float y, audioclip_texture_t* waveformshape, SampleMethod method, std::vector<vec2list>& channels) {
	if (sample->nSamples) {
		double samplesPerPx = waveformshape->samplesPerPx;
		const float width = waveformshape->size.x * (1.0f/waveformshape->scaleX);
		const float height = waveformshape->size.y;
		const int nMaxDowns = sample->downsampled.size()+1;
		int nLevel = 0;
		int downsampleScale = 1;
		audioclip_texture_t waveformScaled = *waveformshape;

		while ((samplesPerPx > 64) && nLevel + 1 < nMaxDowns) {
			samplesPerPx /= 2.0;
			nLevel++;
			downsampleScale*=2;
		}
		waveformScaled.sampleBeginOffset /= downsampleScale;
		waveformScaled.sampleBegin /= downsampleScale;
		waveformScaled.sampleEnd /= downsampleScale;
		dbgassert (nLevel == 0 || nLevel-1 < (int)sample->downsampled.size());
		std::vector<samplechannel_t>& smpCh = nLevel == 0 ? sample->samples : sample->downsampled[nLevel-1];
		int stepSize = 1;
		double dres = samplesPerPx;
		while (dres >= 4.0 && stepSize < 64) {
			dres /= 2.0;
			stepSize*=2;
		}

		const double samplePosClip = waveformScaled.sampleBegin;
		const double samplePosRender = waveformScaled.sampleBeginOffset;


		int verticesPerPx = waveformScaled.quality;
		while (dres >= 2.0 && verticesPerPx < 32) {
			dres /= 2.0;
			verticesPerPx *= 2;
		}
		const int sumRange = 0;

		const float channelHeight = height / (float) sample->nChannels;
		const float vOffset = 1.0f / (float) verticesPerPx;
		const float samplesToPx = 1.0f/samplesPerPx;
		const int nVecsEstimate = width * verticesPerPx;
		int nVecsProduced = 0;

		for (int iChannel = 0; iChannel < sample->nChannels; iChannel++) {
			vec2list vecs;
			if (nVecsEstimate > 0)
				vecs.reserve(nVecsEstimate+500);
			const float px = x;
			const float py = y + channelHeight * iChannel + channelHeight / 2.0f;
			const auto& samplesCh = smpCh[iChannel];
			const float* samplesChPtr = samplesCh.data();
			const int32_t lenSamplesCh = (int32_t)samplesCh.size();
			{
				double samplePos = samplePosRender;
				//Quantize start offset to reduce jitter when start offset changes
				//TODO: arithmetic
				while ((int) (std::round(samplePos - samplePosClip)) % stepSize != 0) {
					samplePos++;
				}
				const double renderOffset = math::max(0.0, (double)(samplePosRender - samplePosClip));
				float lastPtX = -vOffset;
//				log_printf("channel %d offset %f\n", iChannel, lastPtX);
				for (; samplePos < waveformScaled.sampleEnd; ) {
					double sampleOffset = math::max(0.0, (double)(samplePos - samplePosClip));
					if (sampleOffset >= lenSamplesCh) { //TODO: no loop!
						//End of sample, render next channel
						break;
					}
					int32_t sampleIdx = std::round(sampleOffset); //TODO: std::round is slow
					dbgassert((int)sampleIdx%stepSize==0);
						float fCurX = (sampleOffset-renderOffset) * samplesToPx;
						if (fCurX >= lastPtX+vOffset) {
							float data = samplesChPtr[sampleIdx];
							int noffset = 0;
							int c = 0;
							for (;noffset<sumRange; noffset++) {
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
							float fY = -data * channelHeight / 2.0f;
//								dbgassert(px + fCurX>0);
							vec2 vec { px + fCurX, py + fY };
							vecs.push_back(std::move(vec));
							if (fCurX >= width) {
								break;
							}
							lastPtX = fCurX;
						}

//						}
					samplePos += stepSize;
				}

			}
			nVecsProduced += vecs.size();
			if (vecs.size()) {
				for (int i = 1; i < (int)vecs.size(); i++) {
					dbgassert(vecs[i].x > vecs[i-1].x);
				}
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
			}
			channels.push_back(std::move(vecs));
//			waveform.channels.push_back(std::move(vecs));
		}

//		my_printf("nvecs: %d\n", nvecs);
//		my_printf("min: %f, max: %f\n", fsMin, fsMax);
	}

}

void tesselateWaveformOld(audiosample_t* sample, float x, float y, audioclip_texture_t* waveformshape, SampleMethod method, std::vector<vec2list>& channels) {
	if (sample->nSamples) {
		float width = waveformshape->size.x;
		float height = waveformshape->size.y;
		int nMaxDowns = sample->downsampled.size()+1;
		int nLevel = 0;
		int downsampleScale = 1;
		audioclip_texture_t waveformScaled = *waveformshape;
		double samplesPerPx = waveformshape->samplesPerPx;
//		float xscale = 1.0f;
//		if (samplesPerPx > MAX_RES) {
//			xscale = MAX_RES/samplesPerPx;
//			width *= samplesPerPx/MAX_RES;
//			samplesPerPx = MAX_RES;
//		}
		width *= 1.0f/waveformshape->scaleX;
//		while ((samplesPerPx > 64) && nLevel + 1 < nMaxDowns) {
////			samplesPerPx /= 2.0;
////			nLevel++;
//			scale <<= 1;
//			break;
//		}
		waveformScaled.sampleBeginOffset /= downsampleScale;
		waveformScaled.sampleBegin /= downsampleScale;
		waveformScaled.sampleEnd /= downsampleScale;
		dbgassert (nLevel == 0 || nLevel-1 < (int)sample->downsampled.size());
		std::vector<samplechannel_t>& smpCh =sample->samples;// nLevel == 0 ? sample->samples : sample->downsampled[nLevel-1];
		int upscale = 1;
		double dres = samplesPerPx;
		while (dres >= 4.0 && upscale < 64) {
			dres /= 2.0;
			upscale*=2;
		}
//		while (samplesPerPx*subsampling < 8 && subsampling*2 < 4) {
//			subsampling *= 2;
//		}
		int64_t renderOffset = waveformScaled.sampleBeginOffset - waveformScaled.sampleBegin;
		int verticesPerPx = waveformScaled.quality;
		while (dres >= 2.0 && verticesPerPx < 32) {
			dres /= 2.0;
			verticesPerPx*=2;
		}
		int sumRange = 4;
//		if (samplesPerPx > 1000) {
//			sumRange = 0;
//		}
//		while (dres >= 2.0 && sumRange < 256) {
//			dres /= 2.0;
//			sumRange*=2;
//			my_printf("sumRange  %d\n", sumRange);
//		}
		float channelHeight = height / (float) sample->nChannels;
		float vOffset = 1.0f / (float) verticesPerPx;
		float samplesToPx = 1.0f/samplesPerPx;
		int nVecs = width * verticesPerPx;
		float fsMin = 0.0f;
		float fsMax = 0.0f;
		int nvecs = 0;
//		my_printf("renderOffset %f upscale %d, res %f vOffset %f\n", renderOffset, upscale, samplesPerPx, vOffset);
		for (int iChannel = 0; iChannel < sample->nChannels; iChannel++) {
			vec2list vecs;
			if (nVecs > 0)
				vecs.reserve(nVecs+500);
			int64_t samplePos = waveformScaled.sampleBeginOffset;
			int64_t sampleOffset = math::max<int64_t>(0, samplePos - waveformScaled.sampleBegin);
			float px = x;
			float py = y + channelHeight * iChannel + channelHeight / 2.0f;
			auto& samplesCh = smpCh[iChannel];
			float* samplesChPtr = samplesCh.data();
			int32_t lenSamplesCh = (int32_t)samplesCh.size();
			switch (method) {
			case SampleMethod::sample_straight:
			{
					int32_t sampleIdxStart = sampleOffset;
					if (sampleIdxStart >= lenSamplesCh) {
						break;
					}
					dbgassert(sampleIdxStart >= 0);
					float first = samplesChPtr[sampleIdxStart];

					float fY = -first * channelHeight / 2.0f;
//					vec2 vec { px, py + fY };
//					vecs.push_back(std::move(vec));
					bool emplacePre = true;
					if (samplePos - waveformScaled.sampleBegin >= 0) {
//						vecs.emplace_back(px, py + fY);
						emplacePre = false;
					}

					//todo: arithmetic
					while ((samplePos - waveformScaled.sampleBegin) % upscale != 0) {
						samplePos++;
					}
					float lastPtX;
					if (emplacePre) {
//						vecs.emplace_back(px, py );
						lastPtX = x;
//						vecs.emplace_back(px + lastPtX, py);
//						lastPtX += vOffset;
					} else {
						lastPtX = (sampleOffset - renderOffset) * samplesToPx;
						lastPtX -= vOffset;
					}
//					samplePos -= samplePos%upscale;
//					samplePos += upscale;
					for (; samplePos < waveformScaled.sampleEnd; ) {
						sampleOffset = math::max<int64_t>(0, (samplePos - waveformScaled.sampleBegin));
						if (sampleOffset >= lenSamplesCh) { //TODO: no loop!
//							sampleOffset = math::max(0.0, sampleOffset-lenSamplesCh);
							break;
						}
						int32_t sampleIdx = std::round(sampleOffset);
						dbgassert((int)sampleIdx%upscale==0);
//						if ((int)sampleIdx%upscale==0) {
							float fCurX = (sampleOffset-renderOffset) * samplesToPx;
							if (fCurX >= lastPtX+vOffset) {
								float data = samplesChPtr[sampleIdx];
								int noffset = 0;
								int c = 0;
								for (;noffset<sumRange; noffset++) {
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
								float fY = -data * channelHeight / 2.0f;
//								dbgassert(px + fCurX>0);
								vec2 vec { px + fCurX, py + fY };
								vecs.push_back(std::move(vec));
								if (fCurX >= width) {
									break;
								}
								lastPtX = fCurX;
							}

//						}
						samplePos += upscale;
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
						sampleOffset = samplePos-waveformScaled.sampleBegin;
						if (sampleOffset >= lenSamplesCh) {
							break;
						}
						int32_t sampleIdx = sampleOffset;
						float data = samplesChPtr[sampleIdx];
						smin = math::min(smin, data);
						smax = math::max(smax, data);
						float fCurX = (sampleOffset-renderOffset)*samplesToPx;
						if (fCurX - lastPtX >= vOffset) {
							float fs = minOrMax ? smin : smax;
							float fY = -fs * channelHeight / 2.0f;
							vec2 v1 { px + fCurX, py + fY };
		//					v1 *= G_SCALE;
							vecs.push_back(std::move(v1));
							fCurX += vOffset/2.0f;
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
						sampleOffset = samplePos-waveformScaled.sampleBegin;
						if (sampleOffset >= lenSamplesCh) {
							break;
						}
						int32_t sampleIdx = sampleOffset;
						float data = samplesChPtr[sampleIdx];
						float fCurX = (sampleOffset-renderOffset)*samplesToPx;
					    float logScalef = log(math::abs(data)+1) / log(2.);
						sum += math::abs(logScalef);
						nCollect++;
						if (fCurX - lastPtX >= vOffset) {
							float f = 0;
							if (nCollect > 0) {
								float avg = sum / (float) nCollect;
								float fs = minOrMax ? -avg : avg;
								float fY = -fs * channelHeight / 2.0f;
								vec2 vec { px + fCurX, py + fY };
								vecs.push_back(std::move(vec));
								f += vOffset/2.0f;
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
						sampleOffset = samplePos-waveformScaled.sampleBegin;
						if (sampleOffset >= lenSamplesCh) {
							break;
						}
						int32_t sampleIdx = sampleOffset;
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
							dbgassert (fCurX > lastX);
							lastX = fCurX;
							float fY1 = -smin * channelHeight / 2.0f;
							float fY2 = -smax * channelHeight / 2.0f;
							float f1 = sampleIdxMin < sampleIdxMax ? fY1 : fY2;
							float f2 = sampleIdxMin < sampleIdxMax ? fY2 : fY1;
							vec2 v1 { px + fCurX, py + f1 };
//							fCurX += vOffset/2.0f;
							fCurX += samplesToPx*4;
							vec2 v2 { px + fCurX, py + f2 };
							dbgassert(v2.x>v1.x);
//							my_printf("v1.x %f\n", v1.x);
//							my_printf("v2.x %f\n", v2.x);
//
//							my_printf("fcurx %f, sampleoffset %f, renderoffset %f samplestopx %f\n", fCurX, sampleOffset, renderOffset, samplesToPx);
							dbgassert(vecs.empty() || vecs.back().x < v1.x);
							vecs.push_back(std::move(v1));
							dbgassert(vecs.empty() || vecs.back().x < v2.x);
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
					for (; samplePos < waveformScaled.sampleEnd; samplePos+=samplesPerPx*vOffset) {
						int64_t sampleOffset = samplePos-waveformScaled.sampleBegin;
						if (sampleOffset >= lenSamplesCh) {
							break;
						}
						int32_t sampleIdx = sampleOffset;
						float data = samplesChPtr[sampleIdx];
						fsMin = math::min(fsMin, data);
						fsMax = math::max(fsMax, data);
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
			if (vecs.size()) {
				for (int i = 1; i < (int)vecs.size(); i++) {
					dbgassert(vecs[i].x > vecs[i-1].x);
				}
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
			}
			channels.push_back(std::move(vecs));
//			waveform.channels.push_back(std::move(vecs));
		}

//		my_printf("nvecs: %d\n", nvecs);
//		my_printf("min: %f, max: %f\n", fsMin, fsMax);
	}

}

