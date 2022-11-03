#include <cmath>
#include <vector>

#include "host/clip/clip.h"
#include "host/project/projectcontroller.h"
#include "math/vec.h"
#include "math/mat.h"
#include "math/seq_math.h"
#include "host/audiosample.h"
#include "tls.h"
#include "types.h"
#include "waveform_render.h"
#include "logging.h"
#include "assert_dbg.h"

using vec2list = std::vector<vec2>;
namespace DAW::Host {

    float FillAudioGetAudioClipSample(clip_t* clip, clip_audio_t& clipAudio, const sample_fades_ref_t& fadeIn, const sample_fades_ref_t& fadeOut, const audiosample_t* sample, channelnum_t ch, samplecount_t samplePos, samplecount_t offsetStartSamples);
}

void tesselateWaveformStraight(audiosample_t* sample, float x, float y, audioclip_texture_t* waveformshape, std::vector<vec2list>& channels) {
    if (sample->nSamples) {
        double samplesPerPx = waveformshape->samplesPerPx;
        const float width   = waveformshape->size.x * (1.0f / waveformshape->scaleX);
        const float height  = waveformshape->size.y;
        const int nMaxDowns = sample->downsampled.size() + 1;
        samplecount_t nLevel          = 0;
        samplecount_t downsampleScale = 1;

        // make a copy
        audioclip_texture_t waveformScaled = *waveformshape;

        while ((samplesPerPx > 64) && nLevel + 1 < nMaxDowns) {
            samplesPerPx /= 2.0;
            nLevel++;
            downsampleScale *= 2;
        }
        waveformScaled.sampleBeginOffset /= downsampleScale;
        waveformScaled.sampleBegin /= downsampleScale;
        waveformScaled.sampleEnd /= downsampleScale;
        dbgassert(nLevel == 0 || nLevel - 1 < samplecount_t(sample->downsampled.size()));

        std::vector<samplechannel_t>& smpCh = nLevel == 0 ? sample->samples : sample->downsampled[nLevel - 1];
        int stepSize                        = 1;
        double dres                         = samplesPerPx;
        while (dres >= 64.0) {
            dres /= 2.0;
            stepSize *= 2;
        }

        const double samplePosClip   = waveformScaled.sampleBegin;
        const double samplePosRender = waveformScaled.sampleBeginOffset;


        int verticesPerPx = waveformScaled.quality;
        // while (dres >= 16.0 && verticesPerPx < 4) {
        //     dres /= 2.0;
        //     stepSize *= 2;
        //     verticesPerPx *= 2;
        // }

        const float channelHeight = height / (float) sample->nChannels;
        const float vOffset       = 1.0f / (float) verticesPerPx;
        const double samplesToPx   = 1.0f / samplesPerPx;
        const auto nVecsEstimate   = math::ceildS32(width * verticesPerPx);
        // int nVecsProduced         = 0;
        auto clipFadeIn = waveformshape->fades[0];
        auto clipFadeOut = waveformshape->fades[1];

        vec2list vecs;
        for (channelnum_t iChannel = 0; iChannel < sample->nChannels; iChannel++) {
            if (nVecsEstimate > 0)
                vecs.reserve(nVecsEstimate + 128);
            const float px             = x;
            const float py             = y + channelHeight * iChannel + channelHeight / 2.0f;
            const auto& samplesCh      = smpCh[iChannel];
            const float* samplesChPtr  = samplesCh.data();
            const int32_t lenSamplesCh = (int32_t) math::min<size_t>(sample->nSamples, samplesCh.size());

            {
                double samplePos = samplePosRender;
                //Quantize start offset to reduce jitter when start offset changes
                auto samplePosQuantized = math::rounddS64(samplePos - samplePosClip) % stepSize;
                samplePos -= samplePosQuantized;
                const double renderOffset = math::max(0.0, (double) (samplePosRender - samplePosClip));
                float lastPtX             = -vOffset;
                //log_lf(Log::L_DEBUG, "channel %d offset %f\n", iChannel, lastPtX);
                float fAbsMax = 0.0f;
                for (; samplePos < waveformScaled.sampleEnd;) {
                    double sampleOffset = math::max(0.0, (double) (samplePos - samplePosClip));
                    if (sampleOffset >= lenSamplesCh) {
                        //End of sample, render next channel
                        break;
                    }
                    samplecount_t sampleIdx = math::rounddS64(sampleOffset);//TODO: std::round is slow
                    dbgassert(sampleIdx % stepSize == 0);
                    float fCurX = float((sampleOffset - renderOffset) * samplesToPx);
                    float fade = 1.0f;
                    for (auto* clipFade : { &clipFadeIn, &clipFadeOut }) {
                        if (sampleIdx * downsampleScale >= clipFade->samplesFadePos && sampleIdx * downsampleScale < clipFade->samplesFadePos + clipFade->samplesFadeDuration) {
                            float fadePos = (sampleIdx * downsampleScale - clipFade->samplesFadePos) / float(clipFade->samplesFadeDuration);
                            fade *= clipFade->shape.sampleCurveOneShot(fadePos);
                        } else if (clipFade == &clipFadeOut && clipFade->samplesFadeDuration && sampleIdx * downsampleScale >= clipFade->samplesFadePos + clipFade->samplesFadeDuration) {
                            fade = 0.0f;
                        } else if (clipFade == &clipFadeIn && clipFade->samplesFadeDuration && sampleIdx * downsampleScale < clipFade->samplesFadePos) {
                            fade = 0.0f;
                        }
                    }
                    const float data = samplesChPtr[sampleIdx] * fade;
                    fAbsMax = math::absMax(fAbsMax, data * fade);
                    if (fCurX >= lastPtX + 1/16.0f) {
                        // if (samplesPerPx >= 256) {
                        //     int sumRange = 0;// samplesPerPx / 32;
                        //     for (int noffset = -sumRange; noffset <= sumRange; noffset++) {
                        //         if (noffset != 0 && sampleIdx + noffset > 0 && sampleIdx + noffset < lenSamplesCh) {
                        //             data = math::absMax(data, samplesChPtr[sampleIdx + noffset]);
                        //         }
                        //     }
                        // }
                        float fY = -fAbsMax * channelHeight / 2.0f;
                        //dbgassert(px + fCurX>0);
                        // vec2 vec{ px + fCurX, py + fY };
                        vecs.emplace_back(px + fCurX, py + fY);
                        if (fCurX >= width) {
                            break;
                        }
                        if (vecs.size() > 50000)break;
                        lastPtX = fCurX;
                        fAbsMax = 0.0f;
                    }
                    samplePos += stepSize;
                }
            }
            // nVecsProduced += vecs.size();

            #ifndef NDEBUG
            if (!vecs.empty()) {
                for (int i = 1; i < (int) vecs.size(); i++) {
                    dbgassert(vecs[i].x > vecs[i - 1].x);
                }
                //// Find and erase close points
                //auto it = vecs.begin();
                //vec2 a  = *it;
                //it++;
                //while (it != vecs.end()) {
                //    vec2 b  = *it;
                //    float c = glm::distance(a, b);
                //    if (c < 0.125f) {
                //        vecs.erase(it);
                //        log_lf(Log::L_DEBUG, "short dist: %f\n", c);
                //        continue;
                //    }
                //    a = b;
                //    it++;
                //}
            }
            #endif
            channels.push_back(std::move(vecs));
        }
    }
}
#if BUILD_DAW_HOST
void tesselateWaveformFromClip(audiosample_t* sample, float x, float y, audioclip_texture_t* waveformshape, std::vector<vec2list>& channels) {
    if (sample->nSamples) {
        double samplesPerPx = waveformshape->samplesPerPx;
        const float width   = waveformshape->size.x * (1.0f / waveformshape->scaleX);
        const float height  = waveformshape->size.y;

        const int nMaxDowns = sample->downsampled.size() + 1;
        samplecount_t nLevel          = 0;
        samplecount_t downsampleScale = 1;

        // make a copy
        audioclip_texture_t waveformScaled = *waveformshape;

        while ((samplesPerPx > 32) && nLevel + 1 < nMaxDowns) {
            samplesPerPx /= 2.0;
            nLevel++;
            downsampleScale *= 2;
        }
        waveformScaled.sampleBeginOffset /= downsampleScale;
        waveformScaled.sampleBegin /= downsampleScale;
        waveformScaled.sampleEnd /= downsampleScale;
        dbgassert(nLevel == 0 || nLevel - 1 < samplecount_t(sample->downsampled.size()));

        // /* fixed readoffset into backing sample */
        sample_fades_ref_t fades[2] = {
            {&waveformshape->fades[0].shape, waveformshape->fades[0].samplesFadePos / downsampleScale, waveformshape->fades[0].samplesFadeDuration / downsampleScale}, 
            {&waveformshape->fades[1].shape, waveformshape->fades[1].samplesFadePos / downsampleScale, waveformshape->fades[1].samplesFadeDuration / downsampleScale}
        };
        int stepSize                        = 1;
        double dres                         = samplesPerPx;
        while (dres >= 64.0) {
            dres /= 2.0;
            stepSize *= 2;
        }

        const double samplePosClip   = waveformScaled.sampleBegin;
        const double samplePosRender = waveformScaled.sampleBeginOffset;


        int verticesPerPx = waveformScaled.quality;

        const float channelHeight = height / (float) sample->nChannels;
        const float vOffset       = 1.0f / (float) verticesPerPx;
        const double samplesToPx   = 1.0f / samplesPerPx;
        const auto nVecsEstimate   = math::ceildS32(width * verticesPerPx);

        auto& loopPos = waveformshape->loopPos;
        DAW::AudioClipFadeLoopProcessor clipSample{
            fades[0],
            fades[1],
            sample,
            loopPos.clipSampleOffset / downsampleScale,
            loopPos.preLoopLen / downsampleScale,
            loopPos.loopStart / downsampleScale,
            loopPos.loopEnd / downsampleScale,
        };
        
        vec2list vecs;
        for (channelnum_t iChannel = 0; iChannel < sample->nChannels; iChannel++) {
            vecs.clear();
            if (nVecsEstimate > 0)
                vecs.reserve(nVecsEstimate + 128);
            const float px = x;
            const float py = y + channelHeight * iChannel + channelHeight / 2.0f;
            {
                double samplePos = samplePosRender;
                //Quantize start offset to reduce jitter when start offset changes
                auto samplePosQuantized = math::rounddS64(samplePos - samplePosClip) % stepSize;
                samplePos -= samplePosQuantized;
                
                const double renderOffset = math::max(0.0, (double) (samplePosRender - samplePosClip));
                float lastPtX             = -vOffset;
                float fAbsMax = 0.0f;
                for (; samplePos < waveformScaled.sampleEnd;) {
                    double sampleOffset = math::max(0.0, (double) (samplePos - samplePosClip));
                    samplecount_t sampleIdx = math::rounddS64(sampleOffset);//TODO: std::round is slow
                    dbgassert(sampleIdx % stepSize == 0);
                    float fCurX = float((sampleOffset - renderOffset) * samplesToPx);
                    int32_t status = 0;
                    const float data = clipSample.getDownsampled(iChannel, sampleIdx, nLevel, status);
                    if (status != 0) {
                        fAbsMax = math::absMax(fAbsMax, data);
                        if (fCurX >= lastPtX + 1/16.0f) {
                            float fY = -fAbsMax * channelHeight / 2.0f;
                            vecs.emplace_back(px + fCurX, py + fY);
                            if (fCurX >= width) {
                                break;
                            }
                            if (vecs.size() > 50000)break;
                            lastPtX = fCurX;
                            fAbsMax = 0.0f;
                        }
                        samplePos += stepSize;
                    } else {
                        if (!vecs.empty()) {
                            channels.emplace_back(vecs);
                            vecs.clear();
                        }
                    }
                    samplePos += stepSize;
                }
            }
            if (!vecs.empty()) {
                channels.emplace_back(vecs);
                vecs.clear();
            }
        }
    }
}
#endif
/**
 * Does use downsampled data
 * @param sample
 * @param x
 * @param y
 * @param waveformshape
 * @param channels
 */
void tesselateWaveformEnergy(audiosample_t* sample, float x, float y, audioclip_texture_t* waveformshape, std::vector<vec2list>& channels) {
    if (sample->nSamples) {
        // double samplesPerPx       = waveformshape->samplesPerPx;
        const float width         = waveformshape->size.x * (1.0f / waveformshape->scaleX);
        const auto widthPxRounded = math::floorfS32(width);
        const float height        = waveformshape->size.y;

        audioclip_texture_t waveformScaled  = *waveformshape;
        std::vector<samplechannel_t>& smpCh = sample->samples;


        const double samplePosClip   = waveformScaled.sampleBegin;
        const double samplePosRender = waveformScaled.sampleBeginOffset;


        const float channelHeight = height / (float) sample->nChannels;
        const int64_t nSamples    = waveformScaled.sampleEnd - waveformScaled.sampleBeginOffset;


        for (channelnum_t iChannel = 0; iChannel < sample->nChannels; iChannel++) {
            const auto& samplesCh     = smpCh[iChannel];
            const float* samplesChPtr = samplesCh.data();
            const int32_t lenSamplesCh = (int32_t) math::min<size_t>(sample->nSamples, samplesCh.size());
            std::vector<float> lows, highs, energies;
            for (int64_t pixelPos = 0; pixelPos < widthPxRounded; pixelPos++) {
                int64_t startIndex = math::roundfS32(pixelPos / (float) widthPxRounded * nSamples);
                int64_t endIndex   = math::roundfS32((pixelPos + 1) / (float) widthPxRounded * nSamples);
                //dbgassert(startIndex >= 0 && endIndex <= lenSamplesCh);
                float sum2 = 0, low = 0, high = 0;
                for (int64_t samplePos = startIndex; samplePos < endIndex && samplePos < lenSamplesCh; samplePos++) {
                    double sampleOffset  = math::max(0.0, (double) (samplePos + (samplePosRender - samplePosClip)));
                    int64_t samplePosAbs = std::round(sampleOffset);//TODO: std::round is slow
                    if (samplePosAbs < 0)
                        continue;
                    if (samplePosAbs >= lenSamplesCh)
                        break;
                    float fSample = samplesChPtr[samplePosAbs];
                    sum2 += fSample * fSample;
                    if (fSample < low) low = fSample;
                    if (fSample > high) high = fSample;
                }
                float level3 = sqrtf(sum2 / (endIndex - startIndex));
                energies.push_back(level3);
                //if (math::abs(low) > math::abs(high)) {
                //    high = low;
                //} else {
                //    low = high;
                //}
                lows.push_back(low);
                highs.push_back(high);
            }
            const float py = y + channelHeight * iChannel;

            vec2list vecs;
            vecs.reserve(energies.size() * 4);
            int64_t lenEnergies = energies.size();
            for (int64_t pixelPos = 0; pixelPos < lenEnergies; pixelPos++) {
                float fY = (0.5 - 0.5 * energies[pixelPos]) * channelHeight;
                vec2 vec{ x + pixelPos, py + fY };
                vecs.push_back(std::move(vec));
            }
            for (int64_t pixelPos = energies.size() - 1; pixelPos >= 0; pixelPos--) {
                float fY = (0.5 + 0.5 * energies[pixelPos]) * channelHeight;
                vec2 vec{ x + pixelPos, py + fY };
                vecs.push_back(std::move(vec));
            }
            channels.push_back(std::move(vecs));
        }
    }
}

void tesselateWaveform(audiosample_t* sample, float x, float y, audioclip_texture_t* waveformshape, SampleMethod method, std::vector<vec2list>& channels) {
    switch (method) {
        case SampleMethod::sample_clip:
#if BUILD_DAW_HOST
            tesselateWaveformFromClip(sample, x, y, waveformshape, channels);
            break;
#endif
        case SampleMethod::sample_straight:
            tesselateWaveformStraight(sample, x, y, waveformshape, channels);
            break;
        case SampleMethod::sample_energy:
            tesselateWaveformEnergy(sample, x, y, waveformshape, channels);
            break;
        default:
            dbgassert(0);
            break;
    }
}
