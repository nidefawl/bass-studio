#include "../airwindows-types.hpp"
#include "assert_dbg.h"
#include "host/plugin/modules.hpp"
#include "rand.hpp"
#include <array>
#include <numbers>

namespace PluginAirWindows {
    class EffectImplGalacticReverb : public IEffectImpl {
    public:

        double iirAL = 0.0;
        double iirBL = 0.0;
        
        double aIL[6480]{};
        double aJL[3660]{};
        double aKL[1720]{};
        double aLL[680]{};
        
        double aAL[9700]{};
        double aBL[6000]{};
        double aCL[2320]{};
        double aDL[940]{};
        
        double aEL[15220]{};
        double aFL[8460]{};
        double aGL[4540]{};
        double aHL[3200]{};
        
        double aML[3111]{};
        double aMR[3111]{};
        double vibML = 0.0, vibMR = 0.0, depthM = 0.0, oldfpd = 0.0;
        
        double feedbackAL = 0.0;
        double feedbackBL = 0.0;
        double feedbackCL = 0.0;
        double feedbackDL = 0.0;
        
        double lastRefL[7]{};
        double thunderL = 0.0;
        
        double iirAR = 0.0;
        double iirBR = 0.0;
        
        double aIR[6480]{};
        double aJR[3660]{};
        double aKR[1720]{};
        double aLR[680]{};
        
        double aAR[9700]{};
        double aBR[6000]{};
        double aCR[2320]{};
        double aDR[940]{};
        
        double aER[15220]{};
        double aFR[8460]{};
        double aGR[4540]{};
        double aHR[3200]{};
        
        double feedbackAR = 0.0;
        double feedbackBR = 0.0;
        double feedbackCR = 0.0;
        double feedbackDR = 0.0;
        
        double lastRefR[7]{};
        double thunderR = 0.0;
        
        int countA = 1;
        int countB = 1;
        int countC = 1;
        int countD = 1;
        int countE = 1;
        int countF = 1;
        int countG = 1;
        int countH = 1;
        int countI = 1;
        int countJ = 1;
        int countK = 1;
        int countL = 1;
        int countM = 1;
        int cycle = 0; //all these ints are shared across channels, not duplicated
        
        double vibM = 0.0;
            
        uint32_t fpdL = 0;
        uint32_t fpdR = 0;

        //parameters. Always 0-1, and we scale/alter them elsewhere.
        float A = 0.5f;
        float B = 0.5f;
        float C = 0.5f;
        float D = 1.0f;
        float E = 1.0f;

        std::array<double, 13> delayTimes = {
            3407.0, 1823.0, 859.0, 331.0, 4801.0, 2909.0, 1153.0, 461.0, 7607.0, 4217.0, 2269.0, 1597.0, 256.0
        };

        // std::array<double, 13> delayTimes = {
        //     100.0, 200.0, 300.0, 400.0, 500.0, 600.0, 700.0, 800.0, 900.0, 1000.0, 1100.0, 1200.0, 1300.0
        // };

        EffectImplGalacticReverb() {
            seq_rand rand;
            A = 0.5;
            B = 0.5;
            C = 0.5;
            D = 1.0;
            E = 1.0;
            vibM = 3.0;
            oldfpd = 429496.7295;
            fpdL = 16386 + rand.rng_bits(30);
            fpdR = 16386 + rand.rng_bits(30);
        }
        String getName() override {
            return "Galactic Reverb";
        }
        PluginType getPluginType() override { return PLUGIN_TYPE_AIRWINDOWS_GALACTIC_1; }
        void registerParams(std::vector<paramentry>& parameterTypes) override {
            parameterTypes.push_back({ 0, "Replace", "%", 0.5f });
            parameterTypes.push_back({ 1, "Brightness", "%", 0.5f });
            parameterTypes.push_back({ 2, "Detune", "%", 0.5f });
            parameterTypes.push_back({ 3, "Bigness", "%", 0.5f });
            parameterTypes.push_back({ 4, "Dry/Wet", "%", 0.5f });
        }
        void setParameters(internalplugin* plugin) override {
            A = plugin->getParamValue(PARAM_OFFSET_IMPL + 0);
            B = plugin->getParamValue(PARAM_OFFSET_IMPL + 1);
            C = plugin->getParamValue(PARAM_OFFSET_IMPL + 2);
            D = plugin->getParamValue(PARAM_OFFSET_IMPL + 3);
            E = plugin->getParamValue(PARAM_OFFSET_IMPL + 4);
        }
        void processReplacing(float** inputs, float** outputs, samplecount_t sampleFrames, samplerate_t sampleRate) override {

            float* in1  = inputs[0];
            float* in2  = inputs[1];
            float* out1 = outputs[0];
            float* out2 = outputs[1];

            double overallscale = 1.0;
            overallscale /= 44100.0;
            overallscale *= sampleRate;

            int cycleEnd = floor(overallscale);
            if (cycleEnd < 1) cycleEnd = 1;
            if (cycleEnd > 4) cycleEnd = 4;
            //this is going to be 2 for 88.1 or 96k, 3 for silly people, 4 for 176 or 192k
            if (cycle > cycleEnd - 1) cycle = cycleEnd - 1;//sanity check

            double regen     = 0.0625 + ((1.0 - A) * 0.0625);
            double attenuate = (1.0 - (regen / 0.125)) * 1.333;
            double lowpass   = pow(1.00001 - (1.0 - B), 2.0) / sqrt(overallscale);
            double drift     = pow(C, 3) * 0.001;
            double size      = (D * 3.77 * 0.5) + 0.05;
            double wet       = 1.0 - (pow(1.0 - E, 3));

            auto clampToArraySize = [](double delay, int32_t size) {
                return std::clamp(int32_t(delay), 0, size - 1);
            };
            int32_t delayI = clampToArraySize(delayTimes[0] * size, sizeof(aIL) / sizeof(aIL[0]));
            int32_t delayJ = clampToArraySize(delayTimes[1] * size, sizeof(aJL) / sizeof(aJL[0]));
            int32_t delayK = clampToArraySize(delayTimes[2] * size, sizeof(aKL) / sizeof(aKL[0]));
            int32_t delayL = clampToArraySize(delayTimes[3] * size, sizeof(aLL) / sizeof(aLL[0]));
            int32_t delayA = clampToArraySize(delayTimes[4] * size, sizeof(aAL) / sizeof(aAL[0]));
            int32_t delayB = clampToArraySize(delayTimes[5] * size, sizeof(aBL) / sizeof(aBL[0]));
            int32_t delayC = clampToArraySize(delayTimes[6] * size, sizeof(aCL) / sizeof(aCL[0]));
            int32_t delayD = clampToArraySize(delayTimes[7] * size, sizeof(aDL) / sizeof(aDL[0]));
            int32_t delayE = clampToArraySize(delayTimes[8] * size, sizeof(aEL) / sizeof(aEL[0]));
            int32_t delayF = clampToArraySize(delayTimes[9] * size, sizeof(aFL) / sizeof(aFL[0]));
            int32_t delayG = clampToArraySize(delayTimes[10] * size, sizeof(aGL) / sizeof(aGL[0]));
            int32_t delayH = clampToArraySize(delayTimes[11] * size, sizeof(aHL) / sizeof(aHL[0]));
            int32_t delayM = clampToArraySize(delayTimes[12], sizeof(aML) / sizeof(aML[0]));
            dbgassert(size_t(delayI) < sizeof(aIL) / sizeof(aIL[0]));
            dbgassert(size_t(delayJ) < sizeof(aJL) / sizeof(aJL[0]));
            dbgassert(size_t(delayK) < sizeof(aKL) / sizeof(aKL[0]));
            dbgassert(size_t(delayL) < sizeof(aLL) / sizeof(aLL[0]));
            dbgassert(size_t(delayA) < sizeof(aAL) / sizeof(aAL[0]));
            dbgassert(size_t(delayB) < sizeof(aBL) / sizeof(aBL[0]));
            dbgassert(size_t(delayC) < sizeof(aCL) / sizeof(aCL[0]));
            dbgassert(size_t(delayD) < sizeof(aDL) / sizeof(aDL[0]));
            dbgassert(size_t(delayE) < sizeof(aEL) / sizeof(aEL[0]));
            dbgassert(size_t(delayF) < sizeof(aFL) / sizeof(aFL[0]));
            dbgassert(size_t(delayG) < sizeof(aGL) / sizeof(aGL[0]));
            dbgassert(size_t(delayH) < sizeof(aHL) / sizeof(aHL[0]));
            dbgassert(size_t(delayM) < sizeof(aML) / sizeof(aML[0]));


            while (--sampleFrames >= 0) {
                double inputSampleL = *in1;
                double inputSampleR = *in2;
                if (fabs(inputSampleL) < 1.18e-23) inputSampleL = fpdL * 1.18e-17;
                if (fabs(inputSampleR) < 1.18e-23) inputSampleR = fpdR * 1.18e-17;
                double drySampleL = inputSampleL;
                double drySampleR = inputSampleR;

                vibM += (oldfpd * drift);
                if (vibM > (std::numbers::pi * 2.0)) {
                    vibM   = 0.0;
                    oldfpd = 0.4294967295 + (fpdL * 0.0000000000618);
                }

                aML[countM] = inputSampleL * attenuate;
                aMR[countM] = inputSampleR * attenuate;
                countM++;
                if (countM < 0 || countM > delayM) countM = 0;

                double offsetML   = (sin(vibM) + 1.0) * 127;
                double offsetMR   = (sin(vibM + (std::numbers::pi / 2.0)) + 1.0) * 127;
                int workingML     = int(countM + offsetML);
                int workingMR     = int(countM + offsetMR);
                double interpolML = (aML[workingML - ((workingML > delayM) ? delayM + 1 : 0)] * (1 - (offsetML - floor(offsetML))));
                interpolML += (aML[workingML + 1 - ((workingML + 1 > delayM) ? delayM + 1 : 0)] * ((offsetML - floor(offsetML))));
                double interpolMR = (aMR[workingMR - ((workingMR > delayM) ? delayM + 1 : 0)] * (1 - (offsetMR - floor(offsetMR))));
                interpolMR += (aMR[workingMR + 1 - ((workingMR + 1 > delayM) ? delayM + 1 : 0)] * ((offsetMR - floor(offsetMR))));
                inputSampleL = interpolML;
                inputSampleR = interpolMR;
                //predelay that applies vibrato
                //want vibrato speed AND depth like in MatrixVerb

                iirAL        = (iirAL * (1.0 - lowpass)) + (inputSampleL * lowpass);
                inputSampleL = iirAL;
                iirAR        = (iirAR * (1.0 - lowpass)) + (inputSampleR * lowpass);
                inputSampleR = iirAR;
                //initial filter

                cycle++;
                if (cycle == cycleEnd) {//hit the end point and we do a reverb sample
                    dbgassert(countI >= 0 && countI <= int32_t(sizeof(aIL) / sizeof(aIL[0])));
                    dbgassert(countJ >= 0 && countJ <= int32_t(sizeof(aJL) / sizeof(aJL[0])));
                    dbgassert(countK >= 0 && countK <= int32_t(sizeof(aKL) / sizeof(aKL[0])));
                    dbgassert(countL >= 0 && countL <= int32_t(sizeof(aLL) / sizeof(aLL[0])));
                    aIL[countI] = inputSampleL + (feedbackAR * regen);
                    aJL[countJ] = inputSampleL + (feedbackBR * regen);
                    aKL[countK] = inputSampleL + (feedbackCR * regen);
                    aLL[countL] = inputSampleL + (feedbackDR * regen);
                    aIR[countI] = inputSampleR + (feedbackAL * regen);
                    aJR[countJ] = inputSampleR + (feedbackBL * regen);
                    aKR[countK] = inputSampleR + (feedbackCL * regen);
                    aLR[countL] = inputSampleR + (feedbackDL * regen);

                    countI++;
                    if (countI < 0 || countI > delayI) countI = 0;
                    countJ++;
                    if (countJ < 0 || countJ > delayJ) countJ = 0;
                    countK++;
                    if (countK < 0 || countK > delayK) countK = 0;
                    countL++;
                    if (countL < 0 || countL > delayL) countL = 0;

                    double outIL = aIL[countI - ((countI > delayI) ? delayI + 1 : 0)];
                    double outJL = aJL[countJ - ((countJ > delayJ) ? delayJ + 1 : 0)];
                    double outKL = aKL[countK - ((countK > delayK) ? delayK + 1 : 0)];
                    double outLL = aLL[countL - ((countL > delayL) ? delayL + 1 : 0)];
                    double outIR = aIR[countI - ((countI > delayI) ? delayI + 1 : 0)];
                    double outJR = aJR[countJ - ((countJ > delayJ) ? delayJ + 1 : 0)];
                    double outKR = aKR[countK - ((countK > delayK) ? delayK + 1 : 0)];
                    double outLR = aLR[countL - ((countL > delayL) ? delayL + 1 : 0)];
                    //first block: now we have four outputs

                    dbgassert(countA >= 0 && countA <= int(sizeof(aAL) / sizeof(aAL[0])));
                    dbgassert(countB >= 0 && countB <= int(sizeof(aBL) / sizeof(aBL[0])));
                    dbgassert(countC >= 0 && countC <= int(sizeof(aCL) / sizeof(aCL[0])));
                    dbgassert(countD >= 0 && countD <= int(sizeof(aDL) / sizeof(aDL[0])));
                    aAL[countA] = (outIL - (outJL + outKL + outLL));
                    aBL[countB] = (outJL - (outIL + outKL + outLL));
                    aCL[countC] = (outKL - (outIL + outJL + outLL));
                    aDL[countD] = (outLL - (outIL + outJL + outKL));
                    aAR[countA] = (outIR - (outJR + outKR + outLR));
                    aBR[countB] = (outJR - (outIR + outKR + outLR));
                    aCR[countC] = (outKR - (outIR + outJR + outLR));
                    aDR[countD] = (outLR - (outIR + outJR + outKR));

                    countA++;
                    if (countA < 0 || countA > delayA) countA = 0;
                    countB++;
                    if (countB < 0 || countB > delayB) countB = 0;
                    countC++;
                    if (countC < 0 || countC > delayC) countC = 0;
                    countD++;
                    if (countD < 0 || countD > delayD) countD = 0;

                    double outAL = aAL[countA - ((countA > delayA) ? delayA + 1 : 0)];
                    double outBL = aBL[countB - ((countB > delayB) ? delayB + 1 : 0)];
                    double outCL = aCL[countC - ((countC > delayC) ? delayC + 1 : 0)];
                    double outDL = aDL[countD - ((countD > delayD) ? delayD + 1 : 0)];
                    double outAR = aAR[countA - ((countA > delayA) ? delayA + 1 : 0)];
                    double outBR = aBR[countB - ((countB > delayB) ? delayB + 1 : 0)];
                    double outCR = aCR[countC - ((countC > delayC) ? delayC + 1 : 0)];
                    double outDR = aDR[countD - ((countD > delayD) ? delayD + 1 : 0)];
                    //second block: four more outputs

                    aEL[countE] = (outAL - (outBL + outCL + outDL));
                    aFL[countF] = (outBL - (outAL + outCL + outDL));
                    aGL[countG] = (outCL - (outAL + outBL + outDL));
                    aHL[countH] = (outDL - (outAL + outBL + outCL));
                    aER[countE] = (outAR - (outBR + outCR + outDR));
                    aFR[countF] = (outBR - (outAR + outCR + outDR));
                    aGR[countG] = (outCR - (outAR + outBR + outDR));
                    aHR[countH] = (outDR - (outAR + outBR + outCR));

                    countE++;
                    if (countE < 0 || countE > delayE) countE = 0;
                    countF++;
                    if (countF < 0 || countF > delayF) countF = 0;
                    countG++;
                    if (countG < 0 || countG > delayG) countG = 0;
                    countH++;
                    if (countH < 0 || countH > delayH) countH = 0;

                    double outEL = aEL[countE - ((countE > delayE) ? delayE + 1 : 0)];
                    double outFL = aFL[countF - ((countF > delayF) ? delayF + 1 : 0)];
                    double outGL = aGL[countG - ((countG > delayG) ? delayG + 1 : 0)];
                    double outHL = aHL[countH - ((countH > delayH) ? delayH + 1 : 0)];
                    double outER = aER[countE - ((countE > delayE) ? delayE + 1 : 0)];
                    double outFR = aFR[countF - ((countF > delayF) ? delayF + 1 : 0)];
                    double outGR = aGR[countG - ((countG > delayG) ? delayG + 1 : 0)];
                    double outHR = aHR[countH - ((countH > delayH) ? delayH + 1 : 0)];
                    //third block: final outputs

                    feedbackAL = (outEL - (outFL + outGL + outHL));
                    feedbackBL = (outFL - (outEL + outGL + outHL));
                    feedbackCL = (outGL - (outEL + outFL + outHL));
                    feedbackDL = (outHL - (outEL + outFL + outGL));
                    feedbackAR = (outER - (outFR + outGR + outHR));
                    feedbackBR = (outFR - (outER + outGR + outHR));
                    feedbackCR = (outGR - (outER + outFR + outHR));
                    feedbackDR = (outHR - (outER + outFR + outGR));
                    //which we need to feed back into the input again, a bit

                    inputSampleL = (outEL + outFL + outGL + outHL) / 8.0;
                    inputSampleR = (outER + outFR + outGR + outHR) / 8.0;
                    //and take the final combined sum of outputs
                    if (cycleEnd == 4) {
                        lastRefL[0] = lastRefL[4];                     //start from previous last
                        lastRefL[2] = (lastRefL[0] + inputSampleL) / 2;//half
                        lastRefL[1] = (lastRefL[0] + lastRefL[2]) / 2; //one quarter
                        lastRefL[3] = (lastRefL[2] + inputSampleL) / 2;//three quarters
                        lastRefL[4] = inputSampleL;                    //full
                        lastRefR[0] = lastRefR[4];                     //start from previous last
                        lastRefR[2] = (lastRefR[0] + inputSampleR) / 2;//half
                        lastRefR[1] = (lastRefR[0] + lastRefR[2]) / 2; //one quarter
                        lastRefR[3] = (lastRefR[2] + inputSampleR) / 2;//three quarters
                        lastRefR[4] = inputSampleR;                    //full
                    }
                    if (cycleEnd == 3) {
                        lastRefL[0] = lastRefL[3];                                    //start from previous last
                        lastRefL[2] = (lastRefL[0] + lastRefL[0] + inputSampleL) / 3; //third
                        lastRefL[1] = (lastRefL[0] + inputSampleL + inputSampleL) / 3;//two thirds
                        lastRefL[3] = inputSampleL;                                   //full
                        lastRefR[0] = lastRefR[3];                                    //start from previous last
                        lastRefR[2] = (lastRefR[0] + lastRefR[0] + inputSampleR) / 3; //third
                        lastRefR[1] = (lastRefR[0] + inputSampleR + inputSampleR) / 3;//two thirds
                        lastRefR[3] = inputSampleR;                                   //full
                    }
                    if (cycleEnd == 2) {
                        lastRefL[0] = lastRefL[2];                     //start from previous last
                        lastRefL[1] = (lastRefL[0] + inputSampleL) / 2;//half
                        lastRefL[2] = inputSampleL;                    //full
                        lastRefR[0] = lastRefR[2];                     //start from previous last
                        lastRefR[1] = (lastRefR[0] + inputSampleR) / 2;//half
                        lastRefR[2] = inputSampleR;                    //full
                    }
                    if (cycleEnd == 1) {
                        lastRefL[0] = inputSampleL;
                        lastRefR[0] = inputSampleR;
                    }
                    cycle        = 0;//reset
                    inputSampleL = lastRefL[cycle];
                    inputSampleR = lastRefR[cycle];
                } else {
                    inputSampleL = lastRefL[cycle];
                    inputSampleR = lastRefR[cycle];
                    //we are going through our references now
                }

                iirBL        = (iirBL * (1.0 - lowpass)) + (inputSampleL * lowpass);
                inputSampleL = iirBL;
                iirBR        = (iirBR * (1.0 - lowpass)) + (inputSampleR * lowpass);
                inputSampleR = iirBR;
                //end filter

                if (wet < 1.0) {
                    inputSampleL = (inputSampleL * wet) + (drySampleL * (1.0 - wet));
                    inputSampleR = (inputSampleR * wet) + (drySampleR * (1.0 - wet));
                }
#if 0
                //begin 32 bit stereo floating point dither
                int expon;
                frexpf((float) inputSampleL, &expon);
                fpdL ^= fpdL << 13;
                fpdL ^= fpdL >> 17;
                fpdL ^= fpdL << 5;
                inputSampleL += ((double(fpdL) - uint32_t(0x7fffffff)) * 5.5e-36l * pow(2, expon + 62));
                frexpf((float) inputSampleR, &expon);
                fpdR ^= fpdR << 13;
                fpdR ^= fpdR >> 17;
                fpdR ^= fpdR << 5;
                inputSampleR += ((double(fpdR) - uint32_t(0x7fffffff)) * 5.5e-36l * pow(2, expon + 62));
                //end 32 bit stereo floating point dither
#endif
                *out1 = float(inputSampleL);
                *out2 = float(inputSampleR);

                in1++;
                in2++;
                out1++;
                out2++;
            }
        }
    };
    IEffectImpl* createEffectImplGalactic1() {
        return new EffectImplGalacticReverb();
    }
} // namespace PluginAirWindows
