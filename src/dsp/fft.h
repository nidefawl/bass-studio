#pragma once
#include <cstdint>
#include <array>
#include "config.h"
#include "meter.h"
#include <kissfft/kiss_fftr.h>

constexpr float MIN_FREQ = 20;
constexpr float MAX_FREQ = 11000;
// constexpr size_t INPUTLEN = 512*8;


void applyWindowAndPadding(float* in, size_t inLen, std::vector<float>& windowedPadded, int32_t fftlen, float fGain = 1.0f);
void fillbands(std::vector<float> const& mags, std::vector<float> const& freq, std::vector<float>& bands, int32_t fftlen, double srOverFFT);

// using fft_input_array = std::array<float, INPUTLEN>;
template <size_t INPUTLEN>
struct fft_ctxt_t {
    kiss_fftr_cfg cfg;
    const int32_t fftLen;
    const double srOverFFT;
    std::vector<kiss_fft_cpx> tmp{};
    fft_ctxt_t(int32_t _fftLen, double _srOverFFT) : fftLen(_fftLen), srOverFFT(_srOverFFT) {
        tmp.resize(fftLen);
        cfg = kiss_fftr_alloc(fftLen, 0 /*is_inverse_fft*/, NULL, NULL);
        if (!cfg) {
            printf("not enough memory\n");
            exit(1);
        }
    }
    ~fft_ctxt_t() { kiss_fftr_free(cfg); }
    void processFFT(std::vector<float> const& fftIn, std::vector<float>& mags) {
        kiss_fftr(cfg, fftIn.data(), tmp.data());
        auto it = mags.begin();
        float f = 1.0f / (float)INPUTLEN;
        //    float f3 = INPUTLEN / (float)fftLen;
        //    f*=f3;
        for (kiss_fft_cpx& pt : tmp) {
            const float magnitudeSq = pt.r * pt.r + pt.i * pt.i;
            const float f2          = sqrtf(magnitudeSq);
            *it++                   = (2.0f * f2 * f);
        }
    }
};
template <size_t INPUTLEN, int32_t NUM_CHANNELS = OUTPUT_CHANNELS>
struct overlap_buffer_t {
    AudioBlock blockA;
    AudioBlock blockB;
    AudioBlock* dst     = &blockA;
    AudioBlock* dst2    = &blockB;
    int64_t blockOffset = 0;
    const int32_t nChannels;
    overlap_buffer_t() : blockA(NUM_CHANNELS, INPUTLEN), blockB(NUM_CHANNELS, INPUTLEN), nChannels(NUM_CHANNELS) {}
    bool feed(const AudioBlock* block, std::array<std::array<float, INPUTLEN>, NUM_CHANNELS>& ins) {
        bool processBlock = false;
        dst->copyFromPosToPos(block->buf, 0, blockOffset, block->samples, block->channels);

        blockOffset += block->samples;
        if (blockOffset >= INPUTLEN) {
            for (int i = 0; i < NUM_CHANNELS; i++) {
                memcpy(ins[i].data(), dst->buf[i], INPUTLEN * sizeof(float));
            }
            processBlock = true;
            std::swap(dst, dst2);
            // copy second half from previous buffer to new current buffers first half
            dst->copyFromPosToPos(dst2->buf, INPUTLEN >> 1, 0, INPUTLEN >> 1, NUM_CHANNELS);
            // write next block after copied half
            blockOffset = INPUTLEN >> 1;
            // next processing trigger will happen in second feed call after here
        }
        return processBlock;
    }
};
class audio_spectrum {
public:
    const int32_t samplerate;
    const int32_t blocksize;
    const int32_t fftlen;
    const double srOverFFT;
    int32_t numBands;
    int32_t minFreq;
    int32_t maxFreq;
    std::array<std::vector<float>, OUTPUT_CHANNELS> bands{};
    std::array<std::vector<float>, OUTPUT_CHANNELS> mags{};
    audio_spectrum(const audio_spectrum& ref)
        : samplerate(ref.samplerate), blocksize(ref.blocksize), fftlen(ref.fftlen), srOverFFT(ref.srOverFFT), numBands(ref.numBands),
          minFreq(MIN_FREQ), maxFreq(MAX_FREQ) {
        for (int i = 0; i < OUTPUT_CHANNELS; i++) {
            mags[i]  = ref.mags[i];
            bands[i] = ref.bands[i];
        }
    }
    audio_spectrum(const int32_t _blocksize, const int32_t _samplerate, const int32_t _fftLen, const int32_t _numBands)
        : samplerate(_samplerate), blocksize(_blocksize), fftlen(_fftLen), srOverFFT(_samplerate / (double)fftlen), numBands(_numBands),
          minFreq(MIN_FREQ), maxFreq(MAX_FREQ) {}
    void clear() {
        for (int ch = 0; ch < OUTPUT_CHANNELS; ch++) {
            memset(bands[ch].data(), 0, sizeof(float) * bands[ch].size());
            memset(mags[ch].data(), 0, sizeof(float) * mags[ch].size());
        }
    }
};
inline float smoothstep(float a, float b, float x) {
    if (x < a) return 0.0;
    if (x > b) return 1.0;
    float y = (x - a) / (b - a);
    return y * y * (3.0 - 2.0 * y);
}
inline float lerp(float a, float b, float c) {

    return a + (b - a) * c;
}
inline void mixSpectrum(audio_spectrum const* lf, audio_spectrum const* hf, audio_spectrum& out) {
    //  out.samplerate = a->samplerate;
    //  out.blocksize = a->blocksize;
    //  out.fftlen = a->fftlen;
    //  out.srOverFFT = a->srOverFFT;
    //  out.numBands = a->numBands;
    //  assert(a->mags[0].size() == a->fftlen);
    //  assert(b->mags[0].size() == b->fftlen);
    //  assert(a->mags[0].size() == b->fftlen);
    //  assert(out.mags[0].size() == a->mags[0].size());
    //  assert(out.fftlen == a->fftlen);
    for (int i = 0; i < OUTPUT_CHANNELS; i++) {
        out.mags[i] = lf->mags[i];
        //    out.bands[i] = b->bands[i];
    }
    constexpr float fstep = 0.22;
    for (int i = 0; i < OUTPUT_CHANNELS; i++) {
        auto& bandsA = lf->bands[i];
        auto& bandsB = hf->bands[i];
        auto& bandsM = out.bands[i];
        float f      = 1.0f / (lf->numBands - 1);
        for (int j = 0; j < lf->numBands; j++) {
            float fInterp = smoothstep(fstep, 1.0 - fstep, f * j);
            bandsM[j]     = lerp(bandsA[j], bandsB[j], fInterp);
        }
    }
    assert(out.fftlen == out.mags[0].size());
    //  for (int i = 0; i < OUTPUT_CHANNELS; i++) {
    //    auto& A = a->mags[i];
    //    auto& B = b->mags[i];
    //    auto& M = out.mags[i];
    //    float f = 1.0f/(a->fftlen-1);
    //    for (int j = 0; j < a->fftlen; j++) {
    //      float fInterp = smoothstep(fstep, 1.0-fstep, f*j);
    //      M[j] = lerp(A[j], B[j], fInterp);
    //    }
    //  }
}
template <int INPUTLEN, int T>
class fft_processor : public audio_spectrum {
public:
    fft_ctxt_t<INPUTLEN>* fftctxt;
    overlap_buffer_t<INPUTLEN> buffer;
    int blocksProcessed  = 0;
    double processedTime = 0;
    int init             = 0;
    std::array<std::array<float, INPUTLEN>, OUTPUT_CHANNELS> ins{};
    std::vector<float> freq{};
    std::array<DAW::meter_runningsum, OUTPUT_CHANNELS> meterData;
    DAW::rmsmeter meter;
    //  audio_spectrum_t(const int32_t _blocksize, const int32_t _samplerate);
    //  ~audio_spectrum_t();
    //  void setNumBands(int bands);
    //
    //  void processSlice(double time, AudioBlock* block);
    //  void processBuffer(double time, AudioBlock* block);
    //  void onTick(double time);
    //
    //  void getLogScaleBins(std::vector<float>& out, int outputBins, float minFreq, float maxFreq);

    void getLogScaleBins(std::vector<float>& out, int outputBins, float minFreq, float maxFreq) {
        const float minLog10 = log10(minFreq);
        const float maxLog10 = log10(maxFreq);
        const float range    = maxLog10 - minLog10;
        auto it              = out.begin();
        for (int i = 0; i < outputBins; i++) {
            float step = i / (float)outputBins; //-1?
            float freq = powf(10, range * step + minLog10);
            double f   = freq / srOverFFT;
            int binIdx = std::floor(f);
            if (binIdx < 1) continue;
            if (binIdx + 1 >= fftlen / 2 + 1) {
                break;
            }
            float delta = f - binIdx;
            float fout  = 0.0f;
            for (int ch = 0; ch < OUTPUT_CHANNELS; ch++) {
                // bad cache locality, maybe store interleaved for mono mixdown
                auto& channelMags = mags[ch];
                fout += channelMags[binIdx] + (channelMags[binIdx + 1] - channelMags[binIdx]) * delta;
            }
            *it++ = fout / OUTPUT_CHANNELS;
        }
    }

    void setNumBands(int32_t _numBands) { this->numBands = _numBands; }
    void setFreqRange(int32_t _minFreq, int32_t _maxFreq) {
        this->minFreq = _minFreq;
        this->maxFreq = _maxFreq;
    }
    void updateBands() {
        assert(numBands > 0);
        assert(minFreq > 0);
        assert(maxFreq > 0);
        freq.resize(numBands);
        memset(freq.data(), 0, sizeof(float) * freq.size());
        for (int ch = 0; ch < OUTPUT_CHANNELS; ch++) {
            bands[ch].resize(numBands);
            memset(bands[ch].data(), 0, sizeof(float) * bands[ch].size());
        }
        float minLog    = log10f(this->minFreq);
        float maxLog    = log10f(this->maxFreq);
        float bandwidth = 1.0f / numBands;
        for (int i = 0; i < numBands; i++) {
            float min = i / ((float)(numBands)) + 0.5 * bandwidth;
            float fX  = powf(10.0f, (min * (maxLog - minLog)) + minLog);
            freq[i]   = fX;
        }
    }
    fft_processor(const int32_t _blocksize, const int32_t _samplerate)
        : audio_spectrum(_blocksize, _samplerate, INPUTLEN * T, 64),
        fftctxt(new fft_ctxt_t<INPUTLEN>(fftlen, srOverFFT)),
        meter(meterData.data(), static_cast<uint8_t>(meterData.size()))
    {
        updateBands();
        assert(freq.size() == bands[0].size());
        for (int i = 0; i < OUTPUT_CHANNELS; i++) {
            memset(ins[i].data(), 0, sizeof(float) * ins[i].size());
            this->mags[i].resize(this->fftlen);
        }
    }
    ~fft_processor() { delete fftctxt; }
    void onTick(double since) { meter.onTick(since); }
    void processSlice(AudioBlock* block) {
        std::vector<float> paddedInput(this->fftlen);
        for (int i = 0; i < OUTPUT_CHANNELS; i++) {
            if (i > 0) {
                memset(paddedInput.data(), 0, sizeof(float) * paddedInput.size());
            }
            assert(mags[i].size() == this->fftlen && "fftlen must not change at runtime");
            memset(mags[i].data(), 0, sizeof(float) * this->fftlen);
            applyWindowAndPadding(block->buf[i], block->samples, paddedInput, fftlen);
            fftctxt->processFFT(paddedInput, mags[i]);
            fillbands(mags[i], freq, bands[i], fftlen, srOverFFT);
        }
        blocksProcessed++;
    }
    void processBuffer(AudioBlock* block, float fGain = 1.0f) {
        assert(INPUTLEN % block->samples == 0 && "blocksize must be multiple of INPUTLEN");
        assert(block->samples == this->blocksize && "blocksize must not change during runtime");
        meter.update(block, 1.0f);
        bool processBlock = false;
        if (block->samples == INPUTLEN) {
            for (int i = 0; i < OUTPUT_CHANNELS; i++) {
                memcpy(this->ins[i].data(), block->buf[i], INPUTLEN * sizeof(float));
            }
            processBlock = true;
        } else {
            processBlock = buffer.feed(block, this->ins);
        }
        if (processBlock) {
            std::vector<float> paddedInput(this->fftlen);
            for (int i = 0; i < OUTPUT_CHANNELS; i++) {
                assert(mags[i].size() == this->fftlen && "fftlen must not change at runtime");
                memset(mags[i].data(), 0, sizeof(float) * this->fftlen);
                applyWindowAndPadding(ins[i].data(), ins[i].size(), paddedInput, fftlen, fGain);
                fftctxt->processFFT(paddedInput, mags[i]);
                fillbands(mags[i], freq, bands[i], fftlen, srOverFFT);
            }
            blocksProcessed++;
        }
    }
};
