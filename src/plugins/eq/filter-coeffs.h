#pragma once
#include "assert_dbg.h"
#include "host/audiobuffer/audioblock.h"
#include "math/seq_math.h"
#include "types.h"
#include "samplerate.h"
#include <array>
#include <cmath>
#include <complex>

namespace DAW {
    struct FilterCoeffs {
        std::array<double, 32> coefficients{};
        size_t order = 2;

        const double* getCoefficients() const noexcept {
            return coefficients.data();
        }

        size_t getOrder() const noexcept {
            return order;
        }

        void calculateMagnitudes(const std::vector<double>& vecFreqs, std::vector<double>& vecMagsOut, const double sampleRate) const noexcept {
            using complex     = std::complex<double>;
            const complex j   = complex(0, 1) / sampleRate;
            const auto orderFilter  = getOrder();

            std::fill(vecMagsOut.begin(), vecMagsOut.end(), 1.0);
            const auto numFilters = (orderFilter / 2) + (orderFilter % 2);
            for (size_t n = 0; n < numFilters; ++n){
                auto order = size_t(orderFilter > 1 ? 2 : 1);
                const auto* coefs = coefficients.data() + n * 5;
                for (size_t i = 0; i < vecFreqs.size(); ++i) {
                    dbgassert(vecFreqs[i] >= 0 && vecFreqs[i] <= sampleRate * 0.5);
                    const complex jw = std::exp(-M_PI * 2.0 * vecFreqs[i] * j);

                    complex numerator = 0.0;
                    complex factor    = 1.0;
                    for (size_t n = 0; n <= order; ++n) {
                        numerator += coefs[n] * factor;
                        factor *= jw;
                    }

                    complex denominator = 1.0;
                    factor              = jw;

                    for (size_t n = order + 1; n <= 2 * order; ++n) {
                        denominator += coefs[n] * factor;
                        factor *= jw;
                    }

                    vecMagsOut[i] *= std::abs(numerator / denominator);
                }
            }
        }

        static FilterCoeffs CalculateLowPassFirstOrder(double sampleRate,
                                             double frequency) {
            dbgassert(sampleRate > 0.0);
            dbgassert(frequency > 0 && frequency <= static_cast<float>(sampleRate * 0.5));

            auto n  = std::tan(M_PI * frequency / static_cast<double>(sampleRate));
            auto a0 = 1.0 / (n + 1);
            return { { n * a0, n * a0, (n - 1) * a0, 0, 0 }, 1 };
        }

        static FilterCoeffs CalculateLowPassSecondOrder(double sampleRate,
                                             double frequency,
                                             double Q) {
            dbgassert(sampleRate > 0.0);
            dbgassert(frequency > 0 && frequency <= static_cast<float>(sampleRate * 0.5));
            dbgassert(Q > 0.0);

            auto n        = 1 / std::tan(M_PI * frequency / static_cast<double>(sampleRate));
            auto nSquared = n * n;
            auto invQ     = 1 / Q;
            auto c1       = 1 / (1 + invQ * n + nSquared);

            return { { c1, c1 * 2, c1,
                       c1 * 2 * (1 - nSquared),
                       c1 * (1 - invQ * n + nSquared) } };
        }

        static FilterCoeffs CalculateLowPass(double sampleRate,
                                             double frequency,
                                             double Q,
                                             int32_t order) {
            dbgassert(sampleRate > 0.0);
            dbgassert(frequency > 0 && frequency <= static_cast<float>(sampleRate * 0.5));
            dbgassert(Q > 0.0);

            auto n        = 1 / std::tan(M_PI * frequency / static_cast<double>(sampleRate));
            auto nSquared = n * n;

            auto numCoeffs = order / 2;
            FilterCoeffs coeffs{{}, size_t(order)};
            std::array<double, 4> qVals = { Q };
            if (numCoeffs == 2) {
                qVals[0] = 0.7071067811865476;
                qVals[1] = 1.4142135623730951 * Q;
            }
            if (numCoeffs == 4) {
                // qVals[0] = sqrt(0.5);
                // qVals[1] = sqrt(0.5);
                // qVals[2] = sqrt(2.0);
                // qVals[3] = sqrt(2.0) * Q;

                // qVals[0] = 1;
                // qVals[1] = 1;
                // qVals[2] = 1;
                // qVals[3] = Q;

                qVals[0] = 0.50979558;
                qVals[1] = 0.60134489;
                qVals[2] = 0.89997622;
                qVals[3] = 2.5629154;
                qVals[3] *= Q;
            }
            auto itOut = coeffs.coefficients.begin();
            for (int i = 0; i < numCoeffs; ++i) {
                auto qVal = qVals[i];
                auto invQ = 1 / qVal;
                auto c1 = 1 / (1 + invQ * n + nSquared);
                *itOut++ = c1;
                *itOut++ = c1 * 2;
                *itOut++ = c1;
                *itOut++ = c1 * 2 * (1 - nSquared);
                *itOut++ = c1 * (1 - invQ * n + nSquared);
            }
            return coeffs;
        }

        static FilterCoeffs CalculateHighPass(double sampleRate,
                                             double frequency,
                                             double Q,
                                             int32_t order) {
            dbgassert(sampleRate > 0.0);
            dbgassert(frequency > 0 && frequency <= static_cast<float>(sampleRate * 0.5));
            dbgassert(Q > 0.0);

            double omega, sin, cos;
            omega = M_PI * 2.0 * frequency / sampleRate;
            sin   = std::sin(omega);
            cos   = std::cos(omega);

            auto numCoeffs = order / 2;
            FilterCoeffs coeffs{{}, size_t(order)};
            std::array<double, 4> qVals = { Q };
            if (numCoeffs == 2) {
                qVals[0] = 0.7071067811865476;
                qVals[1] = 1.4142135623730951 * Q;
            }
            if (numCoeffs == 4) {
                qVals[0] = 0.50979558;
                qVals[1] = 0.60134489;
                qVals[2] = 0.89997622;
                qVals[3] = 2.5629154;
                qVals[3] *= Q;
            }

            auto itOut = coeffs.coefficients.begin();
            for (int i = 0; i < numCoeffs; ++i) {
                auto qVal = qVals[i];
                auto alpha = sin / (2.0 * qVal);
                auto a0 = 1.0 + alpha;
                auto a1 = -2.0 * cos;
                auto a2 = 1.0 - alpha;
                auto b0 = (1.0 + cos) / 2.0;
                auto b1 = -(1.0 + cos);
                auto b2 = (1.0 + cos) / 2.0;
                *itOut++ = b0 / a0;
                *itOut++ = b1 / a0;
                *itOut++ = b2 / a0;
                *itOut++ = a1 / a0;
                *itOut++ = a2 / a0;
            }

            return coeffs;
        }

        static FilterCoeffs CalculateNotch(double sampleRate,
                                             double frequency,
                                             double Q) {
            dbgassert(sampleRate > 0.0);
            dbgassert(frequency > 0 && frequency <= static_cast<float>(sampleRate * 0.5));
            dbgassert(Q > 0.0);
            auto n = 1 / std::tan (M_PI * frequency / sampleRate);
            auto nSquared = n * n;
            auto invQ = 1 / Q;
            auto c1 = 1 / (1 + n * invQ + nSquared);
            auto b0 = c1 * (1 + nSquared);
            auto b1 = 2 * c1 * (1 - nSquared);

            return { { b0, b1, b0, b1, c1 * (1 - n * invQ + nSquared) } };
        }

        static FilterCoeffs CalculateNotch2(double sampleRate,
                                             double frequency,
                                             double Q) {
            dbgassert(sampleRate > 0.0);
            dbgassert(frequency > 0 && frequency <= static_cast<float>(sampleRate * 0.5));
            dbgassert(Q > 0.0);

            double omega, sin, cos, alpha;
            double a0, a1, a2, b0, b1, b2;
            omega = M_PI * 2.0 * frequency / sampleRate;
            sin   = std::sin(omega);
            cos   = std::cos(omega);
            alpha = sin / (2.0 * Q);

            b0 = 1.0;
            b1 = -2.0 * cos;
            b2 = 1.0;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cos;
            a2 = 1.0 - alpha;

            return { { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 } };
        }

        static FilterCoeffs CalculateBandPass(double sampleRate,
                                              double frequency,
                                              double BW) {
            dbgassert(sampleRate > 0.0);
            dbgassert(frequency > 0 && frequency <= static_cast<float>(sampleRate * 0.5));
            dbgassert(BW > 0.0);

            double omega, sin, cos, alpha;
            double a0, a1, a2, b0, b1, b2;
            omega = M_PI * 2.0 * frequency / sampleRate;
            sin   = std::sin(omega);
            cos   = std::cos(omega);
        
            // alpha = sin / (2.0 * BW);
            alpha = sin * std::sinh(std::log(2.0) / 2.0 * BW * omega / sin);

            // constant skirt gain
            // b0 = sin / 2.0;
            // b1 = 0.0;
            // b2 = -sin / 2;
            // a0 = 1.0 + alpha;
            // a1 = -2.0 * cos;
            // a2 = 1.0 - alpha;

            // Constant peak gain
            b0 = alpha;
            b1 = 0.0;
            b2 = -alpha;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cos;
            a2 = 1.0 - alpha;

            return { { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 } };
        }

        static FilterCoeffs CalculatePeak(double sampleRate,
                                              double frequency,
                                              double BW,
                                              double dbGain) {
            dbgassert(sampleRate > 0.0);
            dbgassert(frequency > 0 && frequency <= static_cast<float>(sampleRate * 0.5));
            dbgassert(BW > 0.0);

            double omega, sin, cos, alpha;
            double a0, a1, a2, b0, b1, b2;
            double a;
            omega = M_PI * 2.0 * frequency / sampleRate;
            sin   = std::sin(omega);
            cos   = std::cos(omega);
        
            // alpha = sin / (2.0 * BW);
            alpha = sin * std::sinh(std::log(2.0) / 2.0 * BW * omega / sin);
            a = std::pow(10.0, dbGain / 40.0);

            b0 = 1.0 + alpha * a;
            b1 = -2.0 * cos;
            b2 = 1.0 - alpha * a;
            a0 = 1.0 + alpha / a;
            a1 = -2.0 * cos;
            a2 = 1.0 - alpha / a;

            return { { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 } };
        }

        static FilterCoeffs CalculateLowShelfNoQ(double sampleRate,
                                              double frequency,
                                              double Q,
                                              double dbGain) {
            dbgassert(sampleRate > 0.0);
            dbgassert(frequency > 0 && frequency <= static_cast<float>(sampleRate * 0.5));
            dbgassert(Q > 0.0);

            double omega, sin, cos;
            double a0, a1, a2, b0, b1, b2;
            double a;
            double beta;
            omega = M_PI * 2.0 * frequency / sampleRate;
            sin   = std::sin(omega);
            cos   = std::cos(omega);
        
            // alpha = sin / (2.0 * Q);
            // alpha = sin * std::sinh(std::log(2.0) / 2.0 * BW * omega / sin);
            a = std::pow(10.0, dbGain / 40.0);
            beta = std::sqrt(a + a);

            b0 = a * ((a + 1.0) -(a - 1.0) * cos + beta * sin);
            b1 = 2.0 * a * ((a - 1.0) - (a + 1.0) * cos);
            b2 = a * ((a + 1.0) - (a - 1.0) * cos - beta * sin);
            a0 = (a + 1.0) + (a - 1.0) * cos + beta * sin;
            a1 = -2.0 * ((a - 1.0) + (a + 1.0) * cos);
            a2 = (a + 1.0) + (a - 1.0) * cos - beta * sin;

            return { { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 } };
        }

        static FilterCoeffs CalculateLowShelf(double sampleRate,
                                              double frequency,
                                              double Q,
                                              double dbGain) {
            dbgassert(sampleRate > 0.0);
            dbgassert(frequency > 0 && frequency <= static_cast<float>(sampleRate * 0.5));
            dbgassert(Q > 0.0);

            auto gainFactor = std::pow(10.0, dbGain / 40.0);
            auto A = math::max (0.0, std::sqrt (gainFactor));
            auto aminus1 = A - 1;
            auto aplus1 = A + 1;
            auto omega = (2 * M_PI * math::max (frequency, 2.0)) / sampleRate;
            auto coso = std::cos (omega);
            auto beta = std::sin (omega) * std::sqrt (A) / Q;
            auto aminus1TimesCoso = aminus1 * coso;
            auto a0 = aplus1 + aminus1TimesCoso + beta;
            if (a0 != 0.0) a0 = 1.0 / a0;
            return { 
                {   
                    A * (aplus1 - aminus1TimesCoso + beta) * a0,
                    A * 2 * (aminus1 - aplus1 * coso) * a0,
                    A * (aplus1 - aminus1TimesCoso - beta) * a0,
                    -2 * (aminus1 + aplus1 * coso) * a0,
                    (aplus1 + aminus1TimesCoso - beta) * a0 
                }
            };
        }

        static FilterCoeffs CalculateHighShelf(double sampleRate,
                                              double frequency,
                                              double Q,
                                              double dbGain) {
            dbgassert(sampleRate > 0.0);
            dbgassert(frequency > 0 && frequency <= static_cast<float>(sampleRate * 0.5));
            dbgassert(Q > 0.0);

            auto gainFactor = std::pow(10.0, dbGain / 40.0);
            auto A = math::max (0.0, std::sqrt (gainFactor));
            auto aminus1 = A - 1;
            auto aplus1 = A + 1;
            auto omega = (2 * M_PI * math::max (frequency, 2.0)) / sampleRate;
            auto coso = std::cos (omega);
            auto beta = std::sin (omega) * std::sqrt (A) / Q;
            auto aminus1TimesCoso = aminus1 * coso;
            auto a0 = aplus1 - aminus1TimesCoso + beta;
            if (a0 != 0.0) a0 = 1.0 / a0;
            return { 
                {   
                    A * (aplus1 + aminus1TimesCoso + beta) * a0,
                    A * -2 * (aminus1 + aplus1 * coso) * a0,
                    A * (aplus1 + aminus1TimesCoso - beta) * a0,
                    2 * (aminus1 - aplus1 * coso) * a0,
                    (aplus1 - aminus1TimesCoso - beta) * a0 
                }
            };
        }
    };

    class Filter {
        std::array<double, 32> state{};

    public:
        void reset() noexcept
        {
            state.fill(0);
        }

        void process1stOrder(const double* coeffs, double* state, const AudioBlock& inputBlock, AudioBlock& outputBlock) noexcept
        {
            dbgassert(inputBlock.channels >= 1);
            dbgassert(outputBlock.channels >= 1);
            dbgassert(outputBlock.samples >= inputBlock.samples);

            auto numSamples = inputBlock.samples;
            auto* src       = inputBlock.buf[0];
            auto* dst       = outputBlock.buf[0];

            auto b0 = coeffs[0];
            auto b1 = coeffs[1];
            auto a1 = coeffs[2];

            auto lv1 = state[0];
            for (samplecount_t i = 0; i < numSamples; ++i) {
                auto input  = src[i];
                auto output = input * b0 + lv1;

                dst[i] = float(output);

                lv1 = (input * b1) - (output * a1);
            }
            state[0] = lv1;
        }

        void process2ndOrder(const double* coeffs, double* state, const AudioBlock& inputBlock, AudioBlock& outputBlock) noexcept
        {
            dbgassert(inputBlock.channels >= 1);
            dbgassert(outputBlock.channels >= 1);
            dbgassert(outputBlock.samples >= inputBlock.samples);

            auto numSamples = inputBlock.samples;
            auto* src       = inputBlock.buf[0];
            auto* dst       = outputBlock.buf[0];
    
            auto b0 = coeffs[0];
            auto b1 = coeffs[1];
            auto b2 = coeffs[2];
            auto a1 = coeffs[3];
            auto a2 = coeffs[4];

            auto lv1 = state[0];
            auto lv2 = state[1];

            for (samplecount_t i = 0; i < numSamples; ++i) {
                auto input  = src[i];
                auto output = (input * b0) + lv1;
                // dbgassert(output > -2.0f && output < 2.0f);
                dst[i]      = float(output);

                lv1 = (input * b1) - (output * a1) + lv2;
                lv2 = (input * b2) - (output * a2);
            }

            state[0] = lv1;
            state[1] = lv2;
        }

        void process (const FilterCoeffs& coefficients, AudioBlock& inputBlock, AudioBlock& outputBlock) noexcept
        {
            auto coeffsBase = coefficients.getCoefficients();
            auto stateBase = state.data();
            switch (coefficients.getOrder()) {
                case 1:
                    process1stOrder(coeffsBase, stateBase, inputBlock, outputBlock);
                    break;
                case 2:
                    process2ndOrder(coeffsBase, stateBase, inputBlock, outputBlock);
                    break;
                case 4:
                    for (int i = 0; i < 2; ++i) {
                        process2ndOrder(coeffsBase, stateBase, inputBlock, outputBlock);
                        stateBase += 2;
                        coeffsBase += 5;
                    }
                    break;
                case 8:
                    for (int i = 0; i < 4; ++i) {
                        process2ndOrder(coeffsBase, stateBase, inputBlock, outputBlock);
                        stateBase += 2;
                        coeffsBase += 5;
                    }
                    break;
                default:
                    dbgassert(false);
            }
        }
    };

} // namespace DAW
