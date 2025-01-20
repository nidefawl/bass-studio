#include "../airwindows-types.hpp"
#include "assert_dbg.h"
#include "rand.hpp"
#include <array>
#include <numbers>

namespace PluginAirWindows {
    const int shortA = 683;
    const int shortB = 2339;
    const int shortC = 2381;
    const int shortD = 887;
    const int shortE = 743;
    const int shortF = 1823;
    const int shortG = 1151;
    const int shortH = 2833;
    const int shortI = 521;
    const int shortJ = 3331;
    const int shortK = 2851;
    const int shortL = 1747;
    const int shortM = 3389;
    const int shortN = 83;
    const int shortO = 443;
    const int shortP = 3221;//290 ms, 10004 seat stadium. Scarcity, 1 in 55796
    class EffectImplGalacticReverb2 : public IEffectImpl {
    public:
        double feedbackA;
        double feedbackB;
        double feedbackC;
        double feedbackD;

        double iirA;
        double iirB;
        double iirC;
        double iirD;
        double iirE;
        double iirF;
        double iirG;
        double iirH;

        double aAL[shortA + 5]{};
        double aBL[shortB + 5]{};
        double aCL[shortC + 5]{};
        double aDL[shortD + 5]{};
        double aEL[shortE + 5]{};
        double aFL[shortF + 5]{};
        double aGL[shortG + 5]{};
        double aHL[shortH + 5]{};
        double aIL[shortI + 5]{};
        double aJL[shortJ + 5]{};
        double aKL[shortK + 5]{};
        double aLL[shortL + 5]{};
        double aML[shortM + 5]{};
        double aNL[shortN + 5]{};
        double aOL[shortO + 5]{};
        double aPL[shortP + 5]{};

        double aAR[shortA + 5]{};
        double aBR[shortB + 5]{};
        double aCR[shortC + 5]{};
        double aDR[shortD + 5]{};
        double aER[shortE + 5]{};
        double aFR[shortF + 5]{};
        double aGR[shortG + 5]{};
        double aHR[shortH + 5]{};
        double aIR[shortI + 5]{};
        double aJR[shortJ + 5]{};
        double aKR[shortK + 5]{};
        double aLR[shortL + 5]{};
        double aMR[shortM + 5]{};
        double aNR[shortN + 5]{};
        double aOR[shortO + 5]{};
        double aPR[shortP + 5]{};

        double feedbackAL;
        double feedbackBL;
        double feedbackCL;
        double feedbackDL;

        double feedbackDR;
        double feedbackHR;
        double feedbackLR;
        double feedbackPR;

        double previousAL;
        double previousBL;
        double previousCL;
        double previousDL;
        double previousEL;

        double lastRefL[7]{};

        double previousAR;
        double previousBR;
        double previousCR;
        double previousDR;
        double previousER;

        double lastRefR[7]{};

        int countAL;
        int countBL;
        int countCL;
        int countDL;
        int countEL;
        int countFL;
        int countGL;
        int countHL;
        int countIL;
        int countJL;
        int countKL;
        int countLL;
        int countML;
        int countNL;
        int countOL;
        int countPL;

        int countAR;
        int countBR;
        int countCR;
        int countDR;
        int countER;
        int countFR;
        int countGR;
        int countHR;
        int countIR;
        int countJR;
        int countKR;
        int countLR;
        int countMR;
        int countNR;
        int countOR;
        int countPR;

        int countZ;

        int cycle;

        double prevMulchAL;
        double prevMulchAR;
        double prevMulchBL;
        double prevMulchBR;
        double prevMulchCL;
        double prevMulchCR;
        double prevMulchDL;
        double prevMulchDR;
        double prevMulchEL;
        double prevMulchER;

        double prevOutAL;
        double prevOutAR;
        double prevOutBL;
        double prevOutBR;
        double prevOutCL;
        double prevOutCR;
        double prevOutDL;
        double prevOutDR;
        double prevOutEL;
        double prevOutER;

        double finalOutAL;
        double finalOutAR;
        double finalOutBL;
        double finalOutBR;
        double finalOutCL;
        double finalOutCR;
        double finalOutDL;
        double finalOutDR;
        double finalOutEL;
        double finalOutER;

        double prevInAL;
        double prevInAR;
        double prevInBL;
        double prevInBR;
        double prevInCL;
        double prevInCR;
        double prevInDL;
        double prevInDR;
        double prevInEL;
        double prevInER;

        double inTrimA;
        double inTrimB;
        double wetA;
        double wetB;

        uint32_t fpdL;
        uint32_t fpdR;
        //default stuff

        float A;
        float B;
        float C;
        float D;

        EffectImplGalacticReverb2() {
            seq_rand rand;

            for (int count = 0; count < shortA + 2; count++) {
                aAL[count] = 0.0;
                aAR[count] = 0.0;
            }
            for (int count = 0; count < shortB + 2; count++) {
                aBL[count] = 0.0;
                aBR[count] = 0.0;
            }
            for (int count = 0; count < shortC + 2; count++) {
                aCL[count] = 0.0;
                aCR[count] = 0.0;
            }
            for (int count = 0; count < shortD + 2; count++) {
                aDL[count] = 0.0;
                aDR[count] = 0.0;
            }
            for (int count = 0; count < shortE + 2; count++) {
                aEL[count] = 0.0;
                aER[count] = 0.0;
            }
            for (int count = 0; count < shortF + 2; count++) {
                aFL[count] = 0.0;
                aFR[count] = 0.0;
            }
            for (int count = 0; count < shortG + 2; count++) {
                aGL[count] = 0.0;
                aGR[count] = 0.0;
            }
            for (int count = 0; count < shortH + 2; count++) {
                aHL[count] = 0.0;
                aHR[count] = 0.0;
            }
            for (int count = 0; count < shortI + 2; count++) {
                aIL[count] = 0.0;
                aIR[count] = 0.0;
            }
            for (int count = 0; count < shortJ + 2; count++) {
                aJL[count] = 0.0;
                aJR[count] = 0.0;
            }
            for (int count = 0; count < shortK + 2; count++) {
                aKL[count] = 0.0;
                aKR[count] = 0.0;
            }
            for (int count = 0; count < shortL + 2; count++) {
                aLL[count] = 0.0;
                aLR[count] = 0.0;
            }
            for (int count = 0; count < shortM + 2; count++) {
                aML[count] = 0.0;
                aMR[count] = 0.0;
            }
            for (int count = 0; count < shortN + 2; count++) {
                aNL[count] = 0.0;
                aNR[count] = 0.0;
            }
            for (int count = 0; count < shortO + 2; count++) {
                aOL[count] = 0.0;
                aOR[count] = 0.0;
            }
            for (int count = 0; count < shortP + 2; count++) {
                aPL[count] = 0.0;
                aPR[count] = 0.0;
            }


            feedbackA = feedbackB = feedbackC = feedbackD = 1.0;

            iirA = iirB = iirC = iirD = iirE = iirF = iirG = iirH = 0.0;

            feedbackAL = 0.0;
            feedbackBL = 0.0;
            feedbackCL = 0.0;
            feedbackDL = 0.0;

            previousAL = 0.0;
            previousBL = 0.0;
            previousCL = 0.0;
            previousDL = 0.0;
            previousEL = 0.0;

            feedbackDR = 0.0;
            feedbackHR = 0.0;
            feedbackLR = 0.0;
            feedbackPR = 0.0;

            previousAR = 0.0;
            previousBR = 0.0;
            previousCR = 0.0;
            previousDR = 0.0;
            previousER = 0.0;

            prevMulchAL = 0.0;
            prevMulchAR = 0.0;
            prevMulchBL = 0.0;
            prevMulchBR = 0.0;
            prevMulchCL = 0.0;
            prevMulchCR = 0.0;
            prevMulchDL = 0.0;
            prevMulchDR = 0.0;
            prevMulchEL = 0.0;
            prevMulchER = 0.0;

            prevOutAL = 0.0;
            prevOutAR = 0.0;
            prevOutBL = 0.0;
            prevOutBR = 0.0;
            prevOutCL = 0.0;
            prevOutCR = 0.0;
            prevOutDL = 0.0;
            prevOutDR = 0.0;
            prevOutEL = 0.0;
            prevOutER = 0.0;

            finalOutAL = 0.0;
            finalOutAR = 0.0;
            finalOutBL = 0.0;
            finalOutBR = 0.0;
            finalOutCL = 0.0;
            finalOutCR = 0.0;
            finalOutDL = 0.0;
            finalOutDR = 0.0;
            finalOutEL = 0.0;
            finalOutER = 0.0;

            prevInAL = 0.0;
            prevInAR = 0.0;
            prevInBL = 0.0;
            prevInBR = 0.0;
            prevInCL = 0.0;
            prevInCR = 0.0;
            prevInDL = 0.0;
            prevInDR = 0.0;
            prevInEL = 0.0;
            prevInER = 0.0;

            for (int count = 0; count < 6; count++) {
                lastRefL[count] = 0.0;
                lastRefR[count] = 0.0;
            }

            countAL = 1;
            countBL = 1;
            countCL = 1;
            countDL = 1;
            countEL = 1;
            countFL = 1;
            countGL = 1;
            countHL = 1;
            countIL = 1;
            countJL = 1;
            countKL = 1;
            countLL = 1;
            countML = 1;
            countNL = 1;
            countOL = 1;
            countPL = 1;

            countAR = 1;
            countBR = 1;
            countCR = 1;
            countDR = 1;
            countER = 1;
            countFR = 1;
            countGR = 1;
            countHR = 1;
            countIR = 1;
            countJR = 1;
            countKR = 1;
            countLR = 1;
            countMR = 1;
            countNR = 1;
            countOR = 1;
            countPR = 1;

            countZ = 1;

            cycle = 0;

            inTrimA = 1.0;
            inTrimB = 1.0;
            wetA    = 0.25;
            wetB    = 0.25;

            A = 0.5;
            B = 0.5;
            C = 0.5;
            D = 1.0;

            fpdL = 16386 + rand.rng_bits(30);
            fpdR = 16386 + rand.rng_bits(30);
        }
        String getName() override {
            return "Galactic 2 Reverb";
        }
        PluginType getPluginType() override { return PLUGIN_TYPE_AIRWINDOWS_GALACTIC_2; }
        void registerParams(std::vector<paramentry>& parameterTypes) override {
            parameterTypes.push_back({ 0, "Drive", "%", 0.5f });
            parameterTypes.push_back({ 1, "Sustain", "%", 0.5f });
            parameterTypes.push_back({ 2, "Darken", "%", 0.5f });
            parameterTypes.push_back({ 3, "Wetness", "%", 0.5f });
        }
        void setParameters(internalplugin* plugin) override {
            A = plugin->getParamValue(PARAM_OFFSET_IMPL + 0);
            B = plugin->getParamValue(PARAM_OFFSET_IMPL + 1);
            C = plugin->getParamValue(PARAM_OFFSET_IMPL + 2);
            D = plugin->getParamValue(PARAM_OFFSET_IMPL + 3);
        }
        void processReplacing(float** inputs, float** outputs, samplecount_t sampleFrames, samplerate_t sampleRate) override {

            float* in1  = inputs[0];
            float* in2  = inputs[1];
            float* out1 = outputs[0];
            float* out2 = outputs[1];

            VstInt32 inFramesToProcess = sampleFrames;//vst doesn't give us this as a separate variable so we'll make it
            double overallscale        = 1.0;
            overallscale /= 44100.0;
            overallscale *= sampleRate;
            int cycleEnd = floor(overallscale);
            if (cycleEnd < 1) cycleEnd = 1;
            if (cycleEnd > 4) cycleEnd = 4;
            //this is going to be 2 for 88.1 or 96k, 3 for silly people, 4 for 176 or 192k
            if (cycle > cycleEnd - 1) cycle = cycleEnd - 1;//sanity check

            inTrimA      = inTrimB;
            inTrimB      = pow(A, 4);
            double regen = 1.0 - (pow(1.0 - B, 4));
            regen *= 0.063;
            double stages = C;
            wetA          = wetB;
            wetB          = 1.0 - (pow(1.0 - D, 4));

            while (--sampleFrames >= 0) {
                double inputSampleL = *in1;
                double inputSampleR = *in2;
                if (fabs(inputSampleL) < 1.18e-23) inputSampleL = fpdL * 1.18e-17;
                if (fabs(inputSampleR) < 1.18e-23) inputSampleR = fpdR * 1.18e-17;
                double drySampleL = inputSampleL;
                double drySampleR = inputSampleR;

                double temp      = (double) sampleFrames / inFramesToProcess;
                double inputGain = (inTrimA * temp) + (inTrimB * (1.0 - temp));
                double wet       = (wetA * temp) + (wetB * (1.0 - temp));
                double outSample;

                cycle++;
                if (cycle == cycleEnd) {//hit the end point and we do a reverb sample
                    if (inputGain < 1.0) {
                        inputSampleL *= inputGain;
                        inputSampleR *= inputGain;
                    }

                    if (stages > 0.858) {
                        outSample    = (inputSampleL + prevInAL) * 0.5;
                        prevInAL     = inputSampleL;
                        inputSampleL = outSample;
                        outSample    = (inputSampleR + prevInAR) * 0.5;
                        prevInAR     = inputSampleR;
                        inputSampleR = outSample;
                    } else {
                        prevInAL = inputSampleL;
                        prevInAR = inputSampleR;
                    }
                    if (stages > 0.660) {
                        outSample    = (inputSampleL + prevInBL) * 0.5;
                        prevInBL     = inputSampleL;
                        inputSampleL = outSample;
                        outSample    = (inputSampleR + prevInBR) * 0.5;
                        prevInBR     = inputSampleR;
                        inputSampleR = outSample;
                    } else {
                        prevInBL = inputSampleL;
                        prevInBR = inputSampleR;
                    }
                    if (stages > 0.462) {
                        outSample    = (inputSampleL + prevInCL) * 0.5;
                        prevInCL     = inputSampleL;
                        inputSampleL = outSample;
                        outSample    = (inputSampleR + prevInCR) * 0.5;
                        prevInCR     = inputSampleR;
                        inputSampleR = outSample;
                    } else {
                        prevInCL = inputSampleL;
                        prevInCR = inputSampleR;
                    }
                    if (stages > 0.264) {
                        outSample    = (inputSampleL + prevInDL) * 0.5;
                        prevInDL     = inputSampleL;
                        inputSampleL = outSample;
                        outSample    = (inputSampleR + prevInDR) * 0.5;
                        prevInDR     = inputSampleR;
                        inputSampleR = outSample;
                    } else {
                        prevInDL = inputSampleL;
                        prevInDR = inputSampleR;
                    }
                    if (stages > 0.066) {
                        outSample    = (inputSampleL + prevInEL) * 0.5;
                        prevInEL     = inputSampleL;
                        inputSampleL = outSample;
                        outSample    = (inputSampleR + prevInER) * 0.5;
                        prevInER     = inputSampleR;
                        inputSampleR = outSample;
                    } else {
                        prevInEL = inputSampleL;
                        prevInER = inputSampleR;
                    }

                    feedbackCL *= 0.0625;
                    feedbackLR *= 0.0625;
                    if (feedbackA < 0.0078125) feedbackA = 0.0078125;
                    if (feedbackA > 1.0) feedbackA = 1.0;
                    if (feedbackB < 0.0078125) feedbackB = 0.0078125;
                    if (feedbackB > 1.0) feedbackB = 1.0;
                    feedbackCL *= feedbackA;
                    feedbackLR *= feedbackB;
                    feedbackA += sin((fabs(feedbackCL * 4) > 1) ? 4 : fabs(feedbackCL * 4)) * pow(feedbackCL, 4);
                    feedbackB += sin((fabs(feedbackLR * 4) > 1) ? 4 : fabs(feedbackLR * 4)) * pow(feedbackLR, 4);
                    feedbackCL *= 16.0;
                    feedbackLR *= 16.0;

                    feedbackDL *= 0.0625;
                    feedbackPR *= 0.0625;
                    if (feedbackC < 0.0078125) feedbackC = 0.0078125;
                    if (feedbackC > 1.0) feedbackC = 1.0;
                    if (feedbackD < 0.0078125) feedbackD = 0.0078125;
                    if (feedbackD > 1.0) feedbackD = 1.0;
                    feedbackDL *= feedbackC;
                    feedbackPR *= feedbackD;
                    feedbackC += sin((fabs(feedbackDL * 4) > 1) ? 4 : fabs(feedbackDL * 4)) * pow(feedbackDL, 4);
                    feedbackD += sin((fabs(feedbackPR * 4) > 1) ? 4 : fabs(feedbackPR * 4)) * pow(feedbackPR, 4);
                    feedbackDL *= 16.0;
                    feedbackPR *= 16.0;

                    double iirAmount = ((feedbackA - 1.0) * -0.00007) + 0.00001;//kick in highpass
                    iirA             = (iirA * (1.0 - iirAmount)) + (feedbackCL * iirAmount);
                    feedbackCL -= iirA;
                    iirE = (iirE * (1.0 - iirAmount)) + (feedbackAL * iirAmount);
                    feedbackAL -= iirE;
                    iirAmount = ((feedbackB - 1.0) * -0.00007) + 0.00001;//kick in highpass
                    iirB      = (iirB * (1.0 - iirAmount)) + (feedbackLR * iirAmount);
                    feedbackLR -= iirB;
                    iirF = (iirF * (1.0 - iirAmount)) + (feedbackDR * iirAmount);
                    feedbackDR -= iirF;
                    iirAmount = ((feedbackC - 1.0) * -0.00007) + 0.00001;//kick in highpass
                    iirC      = (iirC * (1.0 - iirAmount)) + (feedbackDL * iirAmount);
                    feedbackDL -= iirC;
                    iirG = (iirG * (1.0 - iirAmount)) + (feedbackBL * iirAmount);
                    feedbackBL -= iirG;
                    iirAmount = ((feedbackD - 1.0) * -0.00007) + 0.00001;//kick in highpass
                    iirD      = (iirD * (1.0 - iirAmount)) + (feedbackPR * iirAmount);
                    feedbackPR -= iirD;
                    iirH = (iirH * (1.0 - iirAmount)) + (feedbackHR * iirAmount);
                    feedbackHR -= iirH;

                    aAL[countAL] = inputSampleL + (feedbackAL * regen);
                    aBL[countBL] = inputSampleL + (feedbackBL * regen);
                    aCL[countCL] = inputSampleL + (feedbackCL * regen);
                    aDL[countDL] = inputSampleL + (feedbackDL * regen);

                    aDR[countDR] = inputSampleR + (feedbackDR * regen);
                    aHR[countHR] = inputSampleR + (feedbackHR * regen);
                    aLR[countLR] = inputSampleR + (feedbackLR * regen);
                    aPR[countPR] = inputSampleR + (feedbackPR * regen);

                    countAL++;
                    if (countAL < 0 || countAL > shortA) countAL = 0;
                    countBL++;
                    if (countBL < 0 || countBL > shortB) countBL = 0;
                    countCL++;
                    if (countCL < 0 || countCL > shortC) countCL = 0;
                    countDL++;
                    if (countDL < 0 || countDL > shortD) countDL = 0;

                    countDR++;
                    if (countDR < 0 || countDR > shortD) countDR = 0;
                    countHR++;
                    if (countHR < 0 || countHR > shortH) countHR = 0;
                    countLR++;
                    if (countLR < 0 || countLR > shortL) countLR = 0;
                    countPR++;
                    if (countPR < 0 || countPR > shortP) countPR = 0;

                    double outAL = aAL[countAL - ((countAL > shortA) ? shortA + 1 : 0)];
                    double outBL = aBL[countBL - ((countBL > shortB) ? shortB + 1 : 0)];
                    double outCL = aCL[countCL - ((countCL > shortC) ? shortC + 1 : 0)];
                    double outDL = aDL[countDL - ((countDL > shortD) ? shortD + 1 : 0)];

                    double outDR = aDR[countDR - ((countDR > shortD) ? shortD + 1 : 0)];
                    double outHR = aHR[countHR - ((countHR > shortH) ? shortH + 1 : 0)];
                    double outLR = aLR[countLR - ((countLR > shortL) ? shortL + 1 : 0)];
                    double outPR = aPR[countPR - ((countPR > shortP) ? shortP + 1 : 0)];

                    if (stages > 0.792) {
                        outSample   = (outBL + prevMulchAL) * 0.5;
                        prevMulchAL = outBL;
                        outBL       = outSample;
                        outSample   = (outHR + prevMulchAR) * 0.5;
                        prevMulchAR = outHR;
                        outHR       = outSample;
                    } else {
                        prevMulchAL = outBL;
                        prevMulchAR = outHR;
                    }
                    if (stages > 0.990) {
                        outSample   = (outCL + prevMulchEL) * 0.5;
                        prevMulchEL = outCL;
                        outCL       = outSample;
                        outSample   = (outLR + prevMulchER) * 0.5;
                        prevMulchER = outLR;
                        outLR       = outSample;
                    } else {
                        prevMulchEL = outCL;
                        prevMulchER = outLR;
                    }

                    aEL[countEL] = outAL - (outBL + outCL + outDL);
                    aFL[countFL] = outBL - (outAL + outCL + outDL);
                    aGL[countGL] = outCL - (outAL + outBL + outDL);
                    aHL[countHL] = outDL - (outAL + outBL + outCL);

                    aCR[countCR] = outDR - (outHR + outLR + outPR);
                    aGR[countGR] = outHR - (outDR + outLR + outPR);
                    aKR[countKR] = outLR - (outDR + outHR + outPR);
                    aOR[countOR] = outPR - (outDR + outHR + outLR);

                    countEL++;
                    if (countEL < 0 || countEL > shortE) countEL = 0;
                    countFL++;
                    if (countFL < 0 || countFL > shortF) countFL = 0;
                    countGL++;
                    if (countGL < 0 || countGL > shortG) countGL = 0;
                    countHL++;
                    if (countHL < 0 || countHL > shortH) countHL = 0;

                    countCR++;
                    if (countCR < 0 || countCR > shortC) countCR = 0;
                    countGR++;
                    if (countGR < 0 || countGR > shortG) countGR = 0;
                    countKR++;
                    if (countKR < 0 || countKR > shortK) countKR = 0;
                    countOR++;
                    if (countOR < 0 || countOR > shortO) countOR = 0;

                    double outEL = aEL[countEL - ((countEL > shortE) ? shortE + 1 : 0)];
                    double outFL = aFL[countFL - ((countFL > shortF) ? shortF + 1 : 0)];
                    double outGL = aGL[countGL - ((countGL > shortG) ? shortG + 1 : 0)];
                    double outHL = aHL[countHL - ((countHL > shortH) ? shortH + 1 : 0)];

                    double outCR = aCR[countCR - ((countCR > shortC) ? shortC + 1 : 0)];
                    double outGR = aGR[countGR - ((countGR > shortG) ? shortG + 1 : 0)];
                    double outKR = aKR[countKR - ((countKR > shortK) ? shortK + 1 : 0)];
                    double outOR = aOR[countOR - ((countOR > shortO) ? shortO + 1 : 0)];

                    if (stages > 0.594) {
                        outSample   = (outFL + prevMulchBL) * 0.5;
                        prevMulchBL = outFL;
                        outFL       = outSample;
                        outSample   = (outGR + prevMulchBR) * 0.5;
                        prevMulchBR = outGR;
                        outGR       = outSample;
                    } else {
                        prevMulchBL = outFL;
                        prevMulchBR = outGR;
                    }

                    aIL[countIL] = outEL - (outFL + outGL + outHL);
                    aJL[countJL] = outFL - (outEL + outGL + outHL);
                    aKL[countKL] = outGL - (outEL + outFL + outHL);
                    aLL[countLL] = outHL - (outEL + outFL + outGL);

                    aBR[countBR] = outCR - (outGR + outKR + outOR);
                    aFR[countFR] = outGR - (outCR + outKR + outOR);
                    aJR[countJR] = outKR - (outCR + outGR + outOR);
                    aNR[countNR] = outOR - (outCR + outGR + outKR);

                    countIL++;
                    if (countIL < 0 || countIL > shortI) countIL = 0;
                    countJL++;
                    if (countJL < 0 || countJL > shortJ) countJL = 0;
                    countKL++;
                    if (countKL < 0 || countKL > shortK) countKL = 0;
                    countLL++;
                    if (countLL < 0 || countLL > shortL) countLL = 0;

                    countBR++;
                    if (countBR < 0 || countBR > shortB) countBR = 0;
                    countFR++;
                    if (countFR < 0 || countFR > shortF) countFR = 0;
                    countJR++;
                    if (countJR < 0 || countJR > shortJ) countJR = 0;
                    countNR++;
                    if (countNR < 0 || countNR > shortN) countNR = 0;

                    double outIL = aIL[countIL - ((countIL > shortI) ? shortI + 1 : 0)];
                    double outJL = aJL[countJL - ((countJL > shortJ) ? shortJ + 1 : 0)];
                    double outKL = aKL[countKL - ((countKL > shortK) ? shortK + 1 : 0)];
                    double outLL = aLL[countLL - ((countLL > shortL) ? shortL + 1 : 0)];

                    double outBR = aBR[countBR - ((countBR > shortB) ? shortB + 1 : 0)];
                    double outFR = aFR[countFR - ((countFR > shortF) ? shortF + 1 : 0)];
                    double outJR = aJR[countJR - ((countJR > shortJ) ? shortJ + 1 : 0)];
                    double outNR = aNR[countNR - ((countNR > shortN) ? shortN + 1 : 0)];

                    if (stages > 0.396) {
                        outSample   = (outJL + prevMulchCL) * 0.5;
                        prevMulchCL = outJL;
                        outJL       = outSample;
                        outSample   = (outFR + prevMulchCR) * 0.5;
                        prevMulchCR = outFR;
                        outFR       = outSample;
                    } else {
                        prevMulchCL = outJL;
                        prevMulchCR = outFR;
                    }

                    aML[countML] = outIL - (outJL + outKL + outLL);
                    aNL[countNL] = outJL - (outIL + outKL + outLL);
                    aOL[countOL] = outKL - (outIL + outJL + outLL);
                    aPL[countPL] = outLL - (outIL + outJL + outKL);

                    aAR[countAR] = outBR - (outFR + outJR + outNR);
                    aER[countER] = outFR - (outBR + outJR + outNR);
                    aIR[countIR] = outJR - (outBR + outFR + outNR);
                    aMR[countMR] = outNR - (outBR + outFR + outJR);

                    countML++;
                    if (countML < 0 || countML > shortM) countML = 0;
                    countNL++;
                    if (countNL < 0 || countNL > shortN) countNL = 0;
                    countOL++;
                    if (countOL < 0 || countOL > shortO) countOL = 0;
                    countPL++;
                    if (countPL < 0 || countPL > shortP) countPL = 0;

                    countAR++;
                    if (countAR < 0 || countAR > shortA) countAR = 0;
                    countER++;
                    if (countER < 0 || countER > shortE) countER = 0;
                    countIR++;
                    if (countIR < 0 || countIR > shortI) countIR = 0;
                    countMR++;
                    if (countMR < 0 || countMR > shortM) countMR = 0;

                    double outML = aML[countML - ((countML > shortM) ? shortM + 1 : 0)];
                    double outNL = aNL[countNL - ((countNL > shortN) ? shortN + 1 : 0)];
                    double outOL = aOL[countOL - ((countOL > shortO) ? shortO + 1 : 0)];
                    double outPL = aPL[countPL - ((countPL > shortP) ? shortP + 1 : 0)];

                    double outAR = aAR[countAR - ((countAR > shortA) ? shortA + 1 : 0)];
                    double outER = aER[countER - ((countER > shortE) ? shortE + 1 : 0)];
                    double outIR = aIR[countIR - ((countIR > shortI) ? shortI + 1 : 0)];
                    double outMR = aMR[countMR - ((countMR > shortM) ? shortM + 1 : 0)];

                    if (stages > 0.198) {
                        outSample   = (outNL + prevMulchDL) * 0.5;
                        prevMulchDL = outNL;
                        outNL       = outSample;
                        outSample   = (outER + prevMulchDR) * 0.5;
                        prevMulchDR = outER;
                        outER       = outSample;
                    } else {
                        prevMulchDL = outNL;
                        prevMulchDR = outER;
                    }

                    feedbackDR = outML - (outNL + outOL + outPL);
                    feedbackAL = outAR - (outER + outIR + outMR);
                    outSample  = (feedbackDR + feedbackAL) * 0.5;
                    feedbackDR = feedbackAL = outSample;
                    feedbackBL              = outNL - (outML + outOL + outPL);
                    feedbackHR              = outER - (outAR + outIR + outMR);
                    feedbackCL              = outOL - (outML + outNL + outPL);
                    feedbackLR              = outIR - (outAR + outER + outMR);
                    feedbackDL              = outPL - (outML + outNL + outOL);
                    feedbackPR              = outMR - (outAR + outER + outIR);
                    //which we need to feed back into the input again, a bit

                    inputSampleL = (outML + outNL + outOL + outPL) / 8.0;
                    inputSampleR = (outAR + outER + outIR + outMR) / 8.0;
                    //and take the final combined sum of outputs, corrected for Householder gain

                    if (stages > 0.924) {
                        outSample    = (inputSampleL + prevOutAL) * 0.5;
                        prevOutAL    = inputSampleL;
                        inputSampleL = outSample;
                        outSample    = (inputSampleR + prevOutAR) * 0.5;
                        prevOutAR    = inputSampleR;
                        inputSampleR = outSample;
                    } else {
                        prevOutAL = inputSampleL;
                        prevOutAR = inputSampleR;
                    }
                    if (stages > 0.726) {
                        outSample    = (inputSampleL + prevOutBL) * 0.5;
                        prevOutBL    = inputSampleL;
                        inputSampleL = outSample;
                        outSample    = (inputSampleR + prevOutBR) * 0.5;
                        prevOutBR    = inputSampleR;
                        inputSampleR = outSample;
                    } else {
                        prevOutBL = inputSampleL;
                        prevOutBR = inputSampleR;
                    }
                    if (stages > 0.528) {
                        outSample    = (inputSampleL + prevOutCL) * 0.5;
                        prevOutCL    = inputSampleL;
                        inputSampleL = outSample;
                        outSample    = (inputSampleR + prevOutCR) * 0.5;
                        prevOutCR    = inputSampleR;
                        inputSampleR = outSample;
                    } else {
                        prevOutCL = inputSampleL;
                        prevOutCR = inputSampleR;
                    }
                    if (stages > 0.330) {
                        outSample    = (inputSampleL + prevOutDL) * 0.5;
                        prevOutDL    = inputSampleL;
                        inputSampleL = outSample;
                        outSample    = (inputSampleR + prevOutDR) * 0.5;
                        prevOutDR    = inputSampleR;
                        inputSampleR = outSample;
                    } else {
                        prevOutDL = inputSampleL;
                        prevOutDR = inputSampleR;
                    }
                    if (stages > 0.132) {
                        outSample    = (inputSampleL + prevOutEL) * 0.5;
                        prevOutEL    = inputSampleL;
                        inputSampleL = outSample;
                        outSample    = (inputSampleR + prevOutER) * 0.5;
                        prevOutER    = inputSampleR;
                        inputSampleR = outSample;
                    } else {
                        prevOutEL = inputSampleL;
                        prevOutER = inputSampleR;
                    }

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

                inputSampleL *= 0.5;
                inputSampleR *= 0.5;
                if (inputSampleL > 2.0) inputSampleL = 2.0;
                if (inputSampleL < -2.0) inputSampleL = -2.0;
                if (inputSampleR > 2.0) inputSampleR = 2.0;
                if (inputSampleR < -2.0) inputSampleR = -2.0;//clip BigFastArcSin harder
                if (inputSampleL > 0.0) inputSampleL = (inputSampleL * 2.0) / (2.8274333882308 - inputSampleL);
                else
                    inputSampleL = -(inputSampleL * -2.0) / (2.8274333882308 + inputSampleL);
                if (inputSampleR > 0.0) inputSampleR = (inputSampleR * 2.0) / (2.8274333882308 - inputSampleR);
                else
                    inputSampleR = -(inputSampleR * -2.0) / (2.8274333882308 + inputSampleR);
                //BigFastArcSin output stage

                if (stages > 0.924) {
                    outSample    = (inputSampleL + finalOutAL) * 0.5;
                    finalOutAL   = inputSampleL;
                    inputSampleL = outSample;
                    outSample    = (inputSampleR + finalOutAR) * 0.5;
                    finalOutAR   = inputSampleR;
                    inputSampleR = outSample;
                } else {
                    finalOutAL = inputSampleL;
                    finalOutAR = inputSampleR;
                }
                if (stages > 0.726) {
                    outSample    = (inputSampleL + finalOutBL) * 0.5;
                    finalOutBL   = inputSampleL;
                    inputSampleL = outSample;
                    outSample    = (inputSampleR + finalOutBR) * 0.5;
                    finalOutBR   = inputSampleR;
                    inputSampleR = outSample;
                } else {
                    finalOutBL = inputSampleL;
                    finalOutBR = inputSampleR;
                }
                if (stages > 0.528) {
                    outSample    = (inputSampleL + finalOutCL) * 0.5;
                    finalOutCL   = inputSampleL;
                    inputSampleL = outSample;
                    outSample    = (inputSampleR + finalOutCR) * 0.5;
                    finalOutCR   = inputSampleR;
                    inputSampleR = outSample;
                } else {
                    finalOutCL = inputSampleL;
                    finalOutCR = inputSampleR;
                }
                if (stages > 0.330) {
                    outSample    = (inputSampleL + finalOutDL) * 0.5;
                    finalOutDL   = inputSampleL;
                    inputSampleL = outSample;
                    outSample    = (inputSampleR + finalOutDR) * 0.5;
                    finalOutDR   = inputSampleR;
                    inputSampleR = outSample;
                } else {
                    finalOutDL = inputSampleL;
                    finalOutDR = inputSampleR;
                }
                if (stages > 0.132) {
                    outSample    = (inputSampleL + finalOutEL) * 0.5;
                    finalOutEL   = inputSampleL;
                    inputSampleL = outSample;
                    outSample    = (inputSampleR + finalOutER) * 0.5;
                    finalOutER   = inputSampleR;
                    inputSampleR = outSample;
                } else {
                    finalOutEL = inputSampleL;
                    finalOutER = inputSampleR;
                }


                inputSampleL = (inputSampleL * wet) + (drySampleL * (1.0 - wet));
                inputSampleR = (inputSampleR * wet) + (drySampleR * (1.0 - wet));
                //Galactic2 does a proper crossfade so you can perform with it more actively
                if (inputSampleL > 2.0) inputSampleL = 2.0;
                if (inputSampleL < -2.0) inputSampleL = -2.0;
                if (inputSampleR > 2.0) inputSampleR = 2.0;
                if (inputSampleR < -2.0) inputSampleR = -2.0;//catch meltdowns

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

                *out1 = inputSampleL;
                *out2 = inputSampleR;

                in1++;
                in2++;
                out1++;
                out2++;
            }
        }
    };
    IEffectImpl* createEffectImplGalactic2() {
        return new EffectImplGalacticReverb2();
    }
}// namespace PluginAirWindows
