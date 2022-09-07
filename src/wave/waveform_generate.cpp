#include <cmath>
#include <vector>

#include "math/vec.h"
#include "math/mat.h"
#include "math/seq_math.h"
#include "audiosample.h"
#include "waveform_render.h"
#include "logging.h"
#include "assert_dbg.h"

using vec2list = std::vector<vec2>;
using samplechannel_t = std::vector<float>;

void tesselateWaveformStraight(audiosample_t* sample, float x, float y, audioclip_texture_t* waveformshape, std::vector<vec2list>& channels) {
    if (sample->nSamples) {
        double samplesPerPx = waveformshape->samplesPerPx;
        const float width   = waveformshape->size.x * (1.0f / waveformshape->scaleX);
        const float height  = waveformshape->size.y;
        const int nMaxDowns = sample->downsampled.size() + 1;
        int nLevel          = 0;
        int downsampleScale = 1;

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
        dbgassert(nLevel == 0 || nLevel - 1 < (int) sample->downsampled.size());

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
        const float samplesToPx   = 1.0f / samplesPerPx;
        const int nVecsEstimate   = width * verticesPerPx;
        // int nVecsProduced         = 0;

        for (channelnum_t iChannel = 0; iChannel < sample->nChannels; iChannel++) {
            vec2list vecs;
            if (nVecsEstimate > 0)
                vecs.reserve(nVecsEstimate + 500);
            const float px             = x;
            const float py             = y + channelHeight * iChannel + channelHeight / 2.0f;
            const auto& samplesCh      = smpCh[iChannel];
            const float* samplesChPtr  = samplesCh.data();
            const int32_t lenSamplesCh = (int32_t) math::min<size_t>(sample->nSamples, samplesCh.size());

            {
                double samplePos = samplePosRender;
                //Quantize start offset to reduce jitter when start offset changes
                //TODO: arithmetic
                while ((int) (std::round(samplePos - samplePosClip)) % stepSize != 0) {
                    samplePos++;
                }
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
                    int32_t sampleIdx = std::round(sampleOffset);//TODO: std::round is slow
                    dbgassert((int) sampleIdx % stepSize == 0);
                    float fCurX = (sampleOffset - renderOffset) * samplesToPx;
                    const float data = samplesChPtr[sampleIdx];
                    fAbsMax = math::absMax(fAbsMax, data);
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
