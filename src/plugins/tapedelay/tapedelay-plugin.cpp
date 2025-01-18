#include "tapedelay-plugin.hpp"
#include "assert_dbg.h"
#include "gui/automation/automatable.h"
#include "gui/container/container.h"
#include "gui/container/container_layout.h"
#include "gui/controls/textfield.h"
#include "gui/plugin/pluginviewcontainers.h"
#include "host/host.h"
#include "host/plugin/internal/internal-plugin.h"
#include "host/plugin/plugin-lockable.h"
#include "math/seq_math.h"
#include "plugins/lfo/lfo-types.hpp"
#include "plugins/plugin-ui.h"
#include "rand.h"
#include "seq_time.h"
#include "types.h"
#include <array>
#include <numbers>

namespace PluginDelay {
    constexpr double DELAY_MIN_MS = 1.0;
    constexpr double DELAY_MAX_MS = 2000.0;
    double GetScaledDelayMs(float paramValue) {
        auto v = pow(double(paramValue), 4.0);
        v *= (DELAY_MAX_MS - DELAY_MIN_MS);
        v += DELAY_MIN_MS;
        return math::clamp(v, DELAY_MIN_MS, DELAY_MAX_MS);
    }
    float GetParamValueForDelayMs(double ms) {
        auto v = (ms - DELAY_MIN_MS) / (DELAY_MAX_MS - DELAY_MIN_MS);
        return float(pow(v, 0.25));
    }

    class EffectImplDelay : public PluginLockable {
    public:
	
        double dL[88211]{};
        double dR[88211]{};
        double prevSampleL = 0.0;
        double prevSampleR = 0.0;
        double delayL = 0.0;
        double delayR = 0.0;
        double sweepL = 0.0;
        double sweepR = 0.0;
        double regenFilterL[9]{};
        double regenFilterR[9]{};
        double outFilterL[9]{};
        double outFilterR[9]{};
        double lastRefL[10]{};
        double lastRefR[10]{};
        int cycle = 0;
        uint32_t fpdL = 0;
        uint32_t fpdR = 0;
        double paramDelayMs = 100.0;
        float paramFeedback = 0.0;
        float paramFreq = 0.5;
        float paramBW = 0.0;
        float paramFlutter = 0.0;
        float paramDryWet = 1.0;
        bool isPingPong = false;
        int32_t syncFlags;
        std::vector<DAW::LFO::LFOSyncRatio> syncRatios;
        explicit EffectImplDelay(DawInstance* _daw) : PluginLockable(_daw) {
            seq_rand rand;
            fpdL = 16386 + rand.rng_bits(30);
            fpdR = 16386 + rand.rng_bits(30);
            using namespace DAW::LFO;
            syncFlags = STRAIGHT | DOTTED | TRIPLET;
            syncRatios = GetSyncRatios(syncFlags);
        }

        void processReplacing(float** inputs, float** outputs, samplecount_t sampleFrames, samplerate_t sampleRate) {
            float* in1  = inputs[0];
            float* in2  = inputs[1];
            float* out1 = outputs[0];
            float* out2 = outputs[1];

            double overallscale = 1.0;
            overallscale /= 44100.0;
            overallscale *= float(sampleRate);

            int cycleEnd = math::clamp(math::floordS32(overallscale), 1, 4);

            //this is going to be 2 for 88.1 or 96k, 3 for silly people, 4 for 176 or 192k
            if (cycle > cycleEnd - 1) cycle = cycleEnd - 1;//sanity check
            
            double srCorrection = 1.0;
            if (int32_t(sampleRate) == 96000 || int32_t(sampleRate) == 192000) {
                srCorrection = 96000.0 / 88200.0;
            }
            double nSamplesDelay = math::clamp(paramDelayMs * srCorrection, DELAY_MIN_MS, DELAY_MAX_MS) * 0.001 * 44100.0;
            nSamplesDelay = math::max(1.0, nSamplesDelay);
            double baseSpeedL = math::clamp(double(88200.0)/nSamplesDelay, 1.0, 100.0);
            double rightChannelMult = isPingPong ? 2.0 : 1.0;
            double baseSpeedR = math::clamp(baseSpeedL * rightChannelMult, 1.0, 100.0);
            double feedback  = pow(paramFeedback, 2);

            //[0] is frequency: 0.000001 to 0.499999 is near-zero to near-Nyquist
            //[1] is resonance, 0.7071 is Butterworth. Also can't be zero
            regenFilterL[0] = regenFilterR[0] = ((pow(paramFreq, 3) * 0.4) + 0.0001);
            regenFilterL[1] = regenFilterR[1] = pow(paramBW, 2) + 0.01;//resonance

            double K    = tan(std::numbers::pi * regenFilterR[0]);
            double norm = 1.0 / (1.0 + K / regenFilterR[1] + K * K);

            regenFilterL[2] = regenFilterR[2] = K / regenFilterR[1] * norm;
            regenFilterL[4] = regenFilterR[4] = -regenFilterR[2];
            regenFilterL[5] = regenFilterR[5] = 2.0 * (K * K - 1.0) * norm;
            regenFilterL[6] = regenFilterR[6] = (1.0 - K / regenFilterR[1] + K * K) * norm;

            //[0] is frequency: 0.000001 to 0.499999 is near-zero to near-Nyquist
            //[1] is resonance, 0.7071 is Butterworth. Also can't be zero
            outFilterL[0] = outFilterR[0] = regenFilterR[0];
            outFilterL[1] = outFilterR[1] = regenFilterR[1] * std::numbers::phi;//resonance

            K    = tan(std::numbers::pi * outFilterR[0]);
            norm = 1.0 / (1.0 + K / outFilterR[1] + K * K);

            outFilterL[2] = outFilterR[2] = K / outFilterR[1] * norm;
            outFilterL[4] = outFilterR[4] = -outFilterR[2];
            outFilterL[5] = outFilterR[5] = 2.0 * (K * K - 1.0) * norm;
            outFilterL[6] = outFilterR[6] = (1.0 - K / outFilterR[1] + K * K) * norm;

            double vibSpeed = pow(paramFlutter, 5) * baseSpeedL * ((regenFilterR[0] * 0.09) + 0.025);//0.05
            double wet      = math::clamp(paramDryWet * 2.0, 0.0, 1.0);
            double dry      = math::clamp(2.0 - paramDryWet * 2.0, 0.0, 1.0);
            //this echo makes 50% full dry AND full wet, not crossfaded.
            //that's so it can be on submixes without cutting back dry channel when adjusted:
            //unless you go super heavy, you are only adjusting the added echo loudness.

            while (--sampleFrames >= 0) {
                double inputSampleL = *in1;
                double inputSampleR = *in2;
                // if (fabs(inputSampleL) < 1.18e-23) inputSampleL = fpdL * 1.18e-17;
                // if (fabs(inputSampleR) < 1.18e-23) inputSampleR = fpdR * 1.18e-17;
                double drySampleL = inputSampleL;
                double drySampleR = inputSampleR;

                cycle++;
                if (cycle == cycleEnd) {
                    double speedL = baseSpeedL + (vibSpeed * (sin(sweepL) + 1.0));
                    double speedR = baseSpeedR + (vibSpeed * (sin(sweepR) + 1.0));
                    sweepL += (0.05 * inputSampleL * inputSampleL);
                    if (sweepL > std::numbers::pi * 2.0) sweepL -= std::numbers::pi * 2.0;
                    sweepR += (0.05 * inputSampleR * inputSampleR);
                    if (sweepR > std::numbers::pi * 2.0) sweepR -= std::numbers::pi * 2.0;

                    //begin left channel
                    int pos           = floor(delayL);
                    double newSample  = inputSampleL + dL[pos] * feedback;
                    double tempSample = (newSample * regenFilterL[2]) + regenFilterL[7];
                    regenFilterL[7]   = -(tempSample * regenFilterL[5]) + regenFilterL[8];
                    regenFilterL[8]   = (newSample * regenFilterL[4]) - (tempSample * regenFilterL[6]);
                    newSample         = tempSample;

                    delayL -= speedL;
                    if (delayL < 0) delayL += 88200.0;
                    double increment = (newSample - prevSampleL) / speedL;
                    dL[pos]          = prevSampleL;
                    while (pos != floor(delayL)) {
                        dL[pos] = prevSampleL;
                        prevSampleL += increment;
                        pos--;
                        if (pos < 0) pos += 88200;
                    }
                    prevSampleL   = newSample;
                    pos           = floor(delayL);
                    inputSampleL  = dL[pos];
                    tempSample    = (inputSampleL * outFilterL[2]) + outFilterL[7];
                    outFilterL[7] = -(tempSample * outFilterL[5]) + outFilterL[8];
                    outFilterL[8] = (inputSampleL * outFilterL[4]) - (tempSample * outFilterL[6]);
                    inputSampleL  = tempSample;
                    //end left channel
                    //begin right channel
                    pos             = floor(delayR);
                    newSample       = inputSampleR + dR[pos] * feedback;
                    tempSample      = (newSample * regenFilterR[2]) + regenFilterR[7];
                    regenFilterR[7] = -(tempSample * regenFilterR[5]) + regenFilterR[8];
                    regenFilterR[8] = (newSample * regenFilterR[4]) - (tempSample * regenFilterR[6]);
                    newSample       = tempSample;

                    delayR -= speedR;
                    if (delayR < 0) delayR += 88200.0;
                    increment = (newSample - prevSampleR) / speedR;
                    dR[pos]   = prevSampleR;
                    while (pos != floor(delayR)) {
                        dR[pos] = prevSampleR;
                        prevSampleR += increment;
                        pos--;
                        if (pos < 0) pos += 88200;
                    }
                    prevSampleR   = newSample;
                    pos           = floor(delayR);
                    inputSampleR  = dR[pos];
                    tempSample    = (inputSampleR * outFilterR[2]) + outFilterR[7];
                    outFilterR[7] = -(tempSample * outFilterR[5]) + outFilterR[8];
                    outFilterR[8] = (inputSampleR * outFilterR[4]) - (tempSample * outFilterR[6]);
                    inputSampleR  = tempSample;
                    //end right channel

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
                switch (cycleEnd)//multi-pole average using lastRef[] variables
                {
                    case 4:
                        lastRefL[8]  = inputSampleL;
                        inputSampleL = (inputSampleL + lastRefL[7]) * 0.5;
                        lastRefL[7]  = lastRefL[8];//continue, do not break
                        lastRefR[8]  = inputSampleR;
                        inputSampleR = (inputSampleR + lastRefR[7]) * 0.5;
                        lastRefR[7]  = lastRefR[8];//continue, do not break
                    case 3:
                        lastRefL[8]  = inputSampleL;
                        inputSampleL = (inputSampleL + lastRefL[6]) * 0.5;
                        lastRefL[6]  = lastRefL[8];//continue, do not break
                        lastRefR[8]  = inputSampleR;
                        inputSampleR = (inputSampleR + lastRefR[6]) * 0.5;
                        lastRefR[6]  = lastRefR[8];//continue, do not break
                    case 2:
                        lastRefL[8]  = inputSampleL;
                        inputSampleL = (inputSampleL + lastRefL[5]) * 0.5;
                        lastRefL[5]  = lastRefL[8];//continue, do not break
                        lastRefR[8]  = inputSampleR;
                        inputSampleR = (inputSampleR + lastRefR[5]) * 0.5;
                        lastRefR[5]  = lastRefR[8];//continue, do not break
                    case 1:
                    default:
                        break;//no further averaging
                }

                if (wet < 1.0) {
                    inputSampleL *= wet;
                    inputSampleR *= wet;
                }
                if (dry < 1.0) {
                    drySampleL *= dry;
                    drySampleR *= dry;
                }
                inputSampleL += drySampleL;
                inputSampleR += drySampleR;
                //this is our submix echo dry/wet: 0.5 is BOTH at FULL VOLUME
                //purpose is that, if you're adding echo, you're not altering other balances
#if 0
                //begin 32 bit stereo floating point dither
                int expon = 0;
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
        double getBaseSpeed(double paramDelay) {
            return (pow(paramDelay, 4) * 100.0) + 1.0;
        }
        double getFromBaseSpeed(double baseSpeed) {
            return pow((math::clamp(baseSpeed, 1.0, 100.0) - 1.0) / 100.0, 1.0 / 4.0);
        }
        double getCurrentDelayInMilliseconds(double paramDelay) {
            double baseSpeed = getBaseSpeed(paramDelay);
            return (2.0 / baseSpeed) * 1000.0;
        }
        double getCurrentDelayParam(double delayInMilliseconds) {
            if (delayInMilliseconds <= 0.0) {
                return getFromBaseSpeed(0.0);
            }
            double baseSpeed = 2.0 / (delayInMilliseconds / 1000.0);
            return getFromBaseSpeed(baseSpeed);
        }
        bool isPingPongMode() {
            return isPingPong;
        }
    };

    module_delay::module_delay(int32_t _projectGlobalId, IHostCallback* _hostCallback)
        : internalplugin("Tape Delay", _projectGlobalId, _hostCallback),
        impl(new EffectImplDelay(DawInstance::getOptional()))
    {
        struct paramentry {
            int32_t id;
            String name;
            String unit;
            float val;
        };
        const auto parameterTypes = std::array<paramentry, 8> { {
            paramentry{ 0, "Delay", "%", 0.4275f },
            paramentry{ 6, "Ping-Pong", "", 1.0f },
            paramentry{ 7, "Sync", "", 1.0f },
            paramentry{ 1, "Feedback", "%", 0.25f },
            paramentry{ 2, "Frequency", "%", 0.55f },
            paramentry{ 3, "Bandwith", "%", 0.05f },
            paramentry{ 4, "Flutter", "%", 0.3f },
            paramentry{ 5, "Dry/Wet", "%", 0.33f },
        } };
        for (const auto& paramEntry : parameterTypes) {
            registerParam(PARAM_OFFSET_IMPL + paramEntry.id)->initValue(paramEntry);
        }
        getParam(PARAM_OFFSET_IMPL + 6)->quantizationSteps = 1;
    }

    module_delay::~module_delay() {
        delete impl;
    }

    void module_delay::process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) {
        dbgassert( in->samples == format.blockSize
            && out->samples == format.blockSize
            && format.blockSize > 0
            && format.sampleRate > 0);
        if (!assert_expr(in->channels >= 2 && out->channels >= 2)) {
            out->copyFrom(in);
            return;
        }
        const bool bIsSync = getParamValue(PARAM_OFFSET_IMPL + 7) >= 0.5f;
        {
            using namespace DAW::LFO;
            if (bIsSync) {
                impl->syncFlags = STRAIGHT | DOTTED | TRIPLET;
            } else {
                impl->syncFlags = 0;
            }
        }
        const auto bpm100 = host->prjGlobals.tempo100;
        const auto delayParam = getParamValue(PARAM_OFFSET_IMPL + 0);
        if (!bIsSync) {
            impl->paramDelayMs = GetScaledDelayMs(delayParam);
        } else {
            auto index = math::clamp<int32_t>(math::floorfS32(delayParam * CtrSize(impl->syncRatios)), 0, CtrSize(impl->syncRatios) - 1);
            auto ratio = impl->syncRatios[index];
            auto delayTicks = double(TICKS_BAR * ratio.numerator) / ratio.denominator;
            impl->paramDelayMs = toSeconds(delayTicks, bpm100) * 1000.0;
        }
        impl->paramFeedback = getParamValue(PARAM_OFFSET_IMPL + 1);
        impl->paramFreq = getParamValue(PARAM_OFFSET_IMPL + 2);
        impl->paramBW = getParamValue(PARAM_OFFSET_IMPL + 3);
        impl->paramFlutter = getParamValue(PARAM_OFFSET_IMPL + 4);
        impl->paramDryWet = getParamValue(PARAM_OFFSET_IMPL + 5);
        impl->isPingPong = getParamValue(PARAM_OFFSET_IMPL + 6) >= 0.5f;
        impl->processReplacing(in->buf, out->buf, in->samples, format.sampleRate);
    }

    param_converted_t module_delay::convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) {
        auto param = getParam(idx);
        dbgassert(param);
        if (param->idx == PARAM_OFFSET_IMPL + 0) {
            if (impl->syncFlags) {
                auto numSyncRatios = CtrSize(impl->syncRatios);
                for (int32_t i = 0; i < numSyncRatios; ++i) {
                    if (impl->syncRatios[i].text == displayValue.value) {
                        return {((i)/float(numSyncRatios-1)), true};
                    }
                    if (impl->syncRatios[i].text == displayValue.value + "/1") {
                        return {((i)/float(numSyncRatios-1)), true};
                    }
                }
            }
        }
        if (param->idx == PARAM_OFFSET_IMPL + 0) {
            auto fTextFieldVal = static_cast<float>(atof(StringAsCStr(displayValue.value)));
            return { GetParamValueForDelayMs(fTextFieldVal), true };
        }
        return internalplugin::convertParamValueDisplay(idx, displayValue);
    }

    param_unit_t module_delay::convertParamValueToDisplay(int32_t idx, float value) {
        auto param = getParam(idx);
        dbgassert(param);
        if (param->idx == PARAM_OFFSET_IMPL + 0 && impl->syncFlags) {
            auto lfoRateStr = FormatSyncRate(impl->syncRatios, impl->syncFlags, value);
            return {lfoRateStr, impl->syncFlags ? "" : param->unit};
        }
        if (param->idx == PARAM_OFFSET_IMPL + 0) {
            auto ms = GetScaledDelayMs(value);
            auto fmtStr = ms < 10.0 ? "%.2f" : "%.1f";
            return {StringFormat(fmtStr, GetScaledDelayMs(value)), "ms"};
        }
        return internalplugin::convertParamValueToDisplay(idx, value);
    }
    class guictr_module_delay : public guictr_plugin_basic {
        module_delay* const module;
        guictr_stacked ctr_delayoptions;
        guictr_select_enum ctr_delaysync;
        guictr_select_enum ctr_delaymode;
    public:
        explicit guictr_module_delay(module_delay* _module)
            : guictr_plugin_basic(_module),
            module(_module),
            ctr_delaysync(2),
            ctr_delaymode(2)
        {
            const std::array<const char*, 2> stringsDelayMode = {
                "Normal", "Ping-Pong"
            };
            for (size_t i = 0; i < 2; ++i) {
                auto& btn = ctr_delaymode.getButton(i);
                const auto& name = stringsDelayMode[i];
                btn.setTooltipText(String("Select ") + name);
                btn.setText(name);
                btn.setButtonColor(GuiColor::COL_KNOB);
            }
            const std::array<const char*, 2> stringsDelaySync = {
                "Free", "Sync"
            };
            for (size_t i = 0; i < 2; ++i) {
                auto& btn = ctr_delaysync.getButton(i);
                const auto& name = stringsDelaySync[i];
                btn.setTooltipText(String("Tempo ") + name);
                btn.setText(name);
                btn.setButtonColor(GuiColor::COL_KNOB);
            }
            ctr_delayoptions.padding = 0;
            ctr_delayoptions.setBackgroundRendered(false);
            ctr_delayoptions.setVerticalLayout(true);
            
            remove(knobs.front());
            remove(knobs[knobs.size() - 2]);
            remove(knobs[knobs.size() - 1]);
            ctr_delayoptions.addEntry(knobs.front());
            ctr_delayoptions.addEntry(&ctr_delaysync);
            ctr_delayoptions.addEntry(&ctr_delaymode);
            ctr_delayoptions.setSplitters({0.75, 0.75+0.125});
            sortChildren=true;
            ctr_delayoptions.zOrder = 1;
            add(&ctr_delayoptions);
        }
        ~guictr_module_delay() override {
            removeGuis();
        }
            
        class ctxmenu_delay_mode : public ctxtmenu_enum_option_select_base<ctxmenu_enum_select_entry> {
            module_delay* const moduleInstance;
        public:
            ctxmenu_delay_mode(module_delay* _module, String _title, int32_t _id)
                : ctxtmenu_enum_option_select_base(_id, _title), moduleInstance(_module)
            {
                entries.push_back({ 0, "Normal" });
                entries.push_back({ 1, "Ping-Pong" });
                perRowEntries = 2;
            }
            bool isEntrySelected(ctxmenu_enum_select_entry& e) const override {
                return moduleInstance->impl->isPingPongMode() == (e.id == 1);
            }
        };
        class guictr_module_delay_context_menu final : public guictxtmenu {
            module_delay* const module;
        public:
            explicit guictr_module_delay_context_menu(module_delay* _module)
                : guictxtmenu(), module(_module) 
            {
                this->size.x   = 220;
                maxHeight = 0;
                this->fontSize = FONT_SIZE_CTXT_SMALL;
                this->paddingV = 0;
                addEntry(new ctxmenu_delay_mode(module, "Mode", 100));
            }
            bool clickedElement(ctxtmenu_entry* e, int _id) override {
                /* if (_id >= 100) {
                    int clicked = _id - 100;
                    auto lock = module->impl->lock();
                    closeContextMenu();
                    return true;
                } */
                closeContextMenu();
                return false;
            }
        };

        /* void handleRightClick(MouseEvent& evt) override {
            parentCtrl->openContextMenu(new guictr_module_delay_context_menu(module), evt.mousepos);
        } */

        void onGuiOpen() override {
            guictr_plugin_basic::onGuiOpen();
            ctr_delaymode.setAutomationRef(module, PARAM_OFFSET_IMPL + 6);
            ctr_delaysync.setAutomationRef(module, PARAM_OFFSET_IMPL + 7);
        }
    
        void onGuiClose() override {
            guictr_plugin_basic::onGuiClose();
            ctr_delaymode.setAutomationRef(nullptr, -1);
            ctr_delaysync.setAutomationRef(nullptr, -1);
        }
    };
    std::shared_ptr<PluginViewContainer> module_delay::createViewCtrInternal() {
        auto ctr = std::make_shared<PluginViewContainerBasic<guictr_module_delay, module_delay>>(this);
        return ctr;
    }
} // namespace PluginDelay

template<>
effectbase* makeInstance<PluginDelay::module_delay>(int32_t _projectGlobalId, IHostCallback* _hostCallback) {
    return new PluginDelay::module_delay(_projectGlobalId, _hostCallback);
}
