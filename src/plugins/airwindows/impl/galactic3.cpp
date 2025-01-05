#include "../airwindows-types.hpp"
#include "assert_dbg.h"
#include "rand.h"
#include <array>
#include <cstdint>
#include <numbers>

namespace PluginAirWindows {
    class EffectImplGalacticReverb3 : public IEffectImpl {
    public:
        double iirAL;
        double iirBL;

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
        double vibML{}, vibMR{}, depthM{}, oldfpd;

        double feedbackAL;
        double feedbackBL;
        double feedbackCL;
        double feedbackDL;

        double iirAR;
        double iirBR;

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

        double feedbackAR;
        double feedbackBR;
        double feedbackCR;
        double feedbackDR;

        int countA, delayA{};
        int countB, delayB{};
        int countC, delayC{};
        int countD, delayD{};
        int countE, delayE{};
        int countF, delayF{};
        int countG, delayG{};
        int countH, delayH{};
        int countI, delayI{};
        int countJ, delayJ{};
        int countK, delayK{};
        int countL, delayL{};
        int countM, delayM{};//all these ints are shared across channels, not duplicated

        double vibM;

        enum {
            bez_AL,
            bez_AR,
            bez_BL,
            bez_BR,
            bez_CL,
            bez_CR,
            bez_InL,
            bez_InR,
            bez_UnInL,
            bez_UnInR,
            bez_SampL,
            bez_SampR,
            bez_cycle,
            bez_total
        };//the new undersampling. bez signifies the bezier curve reconstruction
        double bez[bez_total]{};

        uint32_t fpdL;
        uint32_t fpdR;
        //default stuff

        float A;
        float B;
        float C;
        float D;
        float E;
        float F;//parameters. Always 0-1, and we scale/alter them elsewhere.

        EffectImplGalacticReverb3() {
            seq_rand rand;

            A = 0.5;
            B = 0.5;
            C = 0.5;
            D = 1.0;
            E = 1.0;
            F = 1.0;

            iirAL = 0.0;
            iirAR = 0.0;
            iirBL = 0.0;
            iirBR = 0.0;

            for (int count = 0; count < 6479; count++) {
                aIL[count] = 0.0;
                aIR[count] = 0.0;
            }
            for (int count = 0; count < 3659; count++) {
                aJL[count] = 0.0;
                aJR[count] = 0.0;
            }
            for (int count = 0; count < 1719; count++) {
                aKL[count] = 0.0;
                aKR[count] = 0.0;
            }
            for (int count = 0; count < 679; count++) {
                aLL[count] = 0.0;
                aLR[count] = 0.0;
            }

            for (int count = 0; count < 9699; count++) {
                aAL[count] = 0.0;
                aAR[count] = 0.0;
            }
            for (int count = 0; count < 5999; count++) {
                aBL[count] = 0.0;
                aBR[count] = 0.0;
            }
            for (int count = 0; count < 2319; count++) {
                aCL[count] = 0.0;
                aCR[count] = 0.0;
            }
            for (int count = 0; count < 939; count++) {
                aDL[count] = 0.0;
                aDR[count] = 0.0;
            }

            for (int count = 0; count < 15219; count++) {
                aEL[count] = 0.0;
                aER[count] = 0.0;
            }
            for (int count = 0; count < 8459; count++) {
                aFL[count] = 0.0;
                aFR[count] = 0.0;
            }
            for (int count = 0; count < 4539; count++) {
                aGL[count] = 0.0;
                aGR[count] = 0.0;
            }
            for (int count = 0; count < 3199; count++) {
                aHL[count] = 0.0;
                aHR[count] = 0.0;
            }

            for (int count = 0; count < 3110; count++) { aML[count] = aMR[count] = 0.0; }

            feedbackAL = 0.0;
            feedbackAR = 0.0;
            feedbackBL = 0.0;
            feedbackBR = 0.0;
            feedbackCL = 0.0;
            feedbackCR = 0.0;
            feedbackDL = 0.0;
            feedbackDR = 0.0;

            countI = 1;
            countJ = 1;
            countK = 1;
            countL = 1;

            countA = 1;
            countB = 1;
            countC = 1;
            countD = 1;

            countE = 1;
            countF = 1;
            countG = 1;
            countH = 1;
            countM = 1;
            //the predelay

            vibM = 3.0;

            oldfpd = 429496.7295;

            for (int x = 0; x < bez_total; x++) bez[x] = 0.0;
            bez[bez_cycle] = 1.0;


            fpdL = 16386 + rand.rng_bits(30);
            fpdR = 16386 + rand.rng_bits(30);
            //this is reset: values being initialized only once. Startup values, whatever they are.
        }
        String getName() override {
            return "Galactic 3 Reverb";
        }
        PluginType getPluginType() override { return PLUGIN_TYPE_AIRWINDOWS_GALACTIC_3; }
        void registerParams(std::vector<paramentry>& parameterTypes) override {
            parameterTypes.push_back({ 0, "Replace", "%", 0.5f });
            parameterTypes.push_back({ 1, "Brightness", "%", 0.5f });
            parameterTypes.push_back({ 2, "Detune", "%", 0.5f });
            parameterTypes.push_back({ 3, "Derez", "%", 1.0f });
            parameterTypes.push_back({ 4, "Bigness", "%", 1.0f });
            parameterTypes.push_back({ 5, "Dry/Wet", "%", 0.5f });
        }
        void setParameters(internalplugin* plugin) override {
            A = plugin->getParam(PARAM_OFFSET_IMPL + 0)->getValue();
            B = plugin->getParam(PARAM_OFFSET_IMPL + 1)->getValue();
            C = plugin->getParam(PARAM_OFFSET_IMPL + 2)->getValue();
            D = plugin->getParam(PARAM_OFFSET_IMPL + 3)->getValue();
            E = plugin->getParam(PARAM_OFFSET_IMPL + 4)->getValue();
            F = plugin->getParam(PARAM_OFFSET_IMPL + 5)->getValue();
        }
        void processReplacing(float** inputs, float** outputs, samplecount_t sampleFrames, samplerate_t sampleRate) override {

            float* in1  = inputs[0];
            float* in2  = inputs[1];
            float* out1 = outputs[0];
            float* out2 = outputs[1];

            double overallscale = 1.0;
            overallscale /= 44100.0;
            overallscale *= sampleRate;

            double regen     = 0.0625 + ((1.0 - A) * 0.0625);
            double attenuate = (1.0 - (regen / 0.125)) * 1.333;
            double lowpass   = pow(1.00001 - (1.0 - B), 2.0) / sqrt(overallscale);
            double drift     = pow(C, 3) * 0.001;
            double derez     = D / overallscale;
            if (derez < 0.0005) derez = 0.0005;
            if (derez > 1.0) derez = 1.0;
            derez = 1.0 / ((int) (1.0 / derez));
            //this hard-locks derez to exact subdivisions of 1.0
            double size = (E * 1.77) + 0.1;
            double wet  = 1.0 - (pow(1.0 - F, 3));

            delayI = 3407.0 * size;
            delayJ = 1823.0 * size;
            delayK = 859.0 * size;
            delayL = 331.0 * size;
            delayA = 4801.0 * size;
            delayB = 2909.0 * size;
            delayC = 1153.0 * size;
            delayD = 461.0 * size;
            delayE = 7607.0 * size;
            delayF = 4217.0 * size;
            delayG = 2269.0 * size;
            delayH = 1597.0 * size;
            delayM = 256;

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
                int32_t workingML = int32_t(countM + offsetML);
                int32_t workingMR = int32_t(countM + offsetMR);
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

                bez[bez_cycle] += derez;
                bez[bez_SampL] += ((inputSampleL + bez[bez_InL]) * derez);
                bez[bez_SampR] += ((inputSampleR + bez[bez_InR]) * derez);
                bez[bez_InL] = inputSampleL;
                bez[bez_InR] = inputSampleR;
                if (bez[bez_cycle] > 1.0) {//hit the end point and we do a reverb sample
                    bez[bez_cycle] = 0.0;

                    aIL[countI]    = (bez[bez_SampL] + bez[bez_UnInL]) + (feedbackAR * regen);
                    aJL[countJ]    = (bez[bez_SampL] + bez[bez_UnInL]) + (feedbackBR * regen);
                    aKL[countK]    = (bez[bez_SampL] + bez[bez_UnInL]) + (feedbackCR * regen);
                    aLL[countL]    = (bez[bez_SampL] + bez[bez_UnInL]) + (feedbackDR * regen);
                    bez[bez_UnInL] = bez[bez_SampL];

                    aIR[countI]    = (bez[bez_SampR] + bez[bez_UnInR]) + (feedbackAL * regen);
                    aJR[countJ]    = (bez[bez_SampR] + bez[bez_UnInR]) + (feedbackBL * regen);
                    aKR[countK]    = (bez[bez_SampR] + bez[bez_UnInR]) + (feedbackCL * regen);
                    aLR[countL]    = (bez[bez_SampR] + bez[bez_UnInR]) + (feedbackDL * regen);
                    bez[bez_UnInR] = bez[bez_SampR];

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

                    bez[bez_CL]    = bez[bez_BL];
                    bez[bez_BL]    = bez[bez_AL];
                    bez[bez_AL]    = inputSampleL;
                    bez[bez_SampL] = 0.0;

                    bez[bez_CR]    = bez[bez_BR];
                    bez[bez_BR]    = bez[bez_AR];
                    bez[bez_AR]    = inputSampleR;
                    bez[bez_SampR] = 0.0;
                }
                double CBL   = (bez[bez_CL] * (1.0 - bez[bez_cycle])) + (bez[bez_BL] * bez[bez_cycle]);
                double CBR   = (bez[bez_CR] * (1.0 - bez[bez_cycle])) + (bez[bez_BR] * bez[bez_cycle]);
                double BAL   = (bez[bez_BL] * (1.0 - bez[bez_cycle])) + (bez[bez_AL] * bez[bez_cycle]);
                double BAR   = (bez[bez_BR] * (1.0 - bez[bez_cycle])) + (bez[bez_AR] * bez[bez_cycle]);
                double CBAL  = (bez[bez_BL] + (CBL * (1.0 - bez[bez_cycle])) + (BAL * bez[bez_cycle])) * 0.125;
                double CBAR  = (bez[bez_BR] + (CBR * (1.0 - bez[bez_cycle])) + (BAR * bez[bez_cycle])) * 0.125;
                inputSampleL = CBAL;
                inputSampleR = CBAR;

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
                inputSampleL += ((double(fpdL) - uint32_t(0x7fffffff)) * 5.5e-36 * pow(2, expon + 62));
                frexpf((float) inputSampleR, &expon);
                fpdR ^= fpdR << 13;
                fpdR ^= fpdR >> 17;
                fpdR ^= fpdR << 5;
                inputSampleR += ((double(fpdR) - uint32_t(0x7fffffff)) * 5.5e-36 * pow(2, expon + 62));
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
    IEffectImpl* createEffectImplGalactic3() {
        return new EffectImplGalacticReverb3();
    }
}// namespace PluginAirWindows
