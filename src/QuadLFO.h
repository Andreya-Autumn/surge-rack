/*
 * SurgeXT for VCV Rack - a Surge Synth Team product
 *
 * Copyright 2019 - 2022, Various authors, as described in the github
 * transaction log.
 *
 * SurgeXT for VCV Rack is released under the Gnu General Public Licence
 * V3 or later (GPL-3.0-or-later). The license is found in the file
 * "LICENSE" in the root of this repository or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * All source for Surge XT for VCV Rack is available at
 * https://github.com/surge-synthesizer/surge-rack/
 */

#include "dsp/SimpleLFO.h"

/*
 * ToDos
 *
 * Module
 *    - Code Interwoven / Spread mode
 *    - Quadrature mode (amplitudes per)
 *    - Clock and tempoSync cases
 *    - Square and S&H get sample accurate transitions in block rather than interps
 *    - Square Wave sample transition sample accurate inside block
 */

#ifndef SURGE_XT_RACK_QUADADHPP
#define SURGE_XT_RACK_QUADADHPP

#include "SurgeXT.h"
#include "dsp/Effect.h"
#include "XTModule.h"
#include "rack.hpp"
#include <cstring>

#include "DebugHelpers.h"
#include "FxPresetAndClipboardManager.h"

#include "LayoutEngine.h"
#include "ADSRModulationSource.h"

namespace sst::surgext_rack::quadlfo
{
struct QuadLFO : modules::XTModule
{
    static constexpr int n_mod_params{8};
    static constexpr int n_mod_inputs{4};
    static constexpr int n_lfos{4};

    enum ParamIds
    {
        RATE_0,
        DEFORM_0 = RATE_0 + n_lfos,
        SHAPE_0 = DEFORM_0 + n_lfos,
        BIPOLAR_0 = SHAPE_0 + n_lfos,
        MOD_PARAM_0 = BIPOLAR_0 + n_lfos,
        INTERPLAY_MODE = MOD_PARAM_0 + n_mod_params * n_mod_inputs,
        NUM_PARAMS
    };

    enum InputIds
    {
        TRIGGER_0,
        CLOCK_IN = TRIGGER_0 + n_lfos,

        MOD_INPUT_0,
        NUM_INPUTS = MOD_INPUT_0 + n_mod_inputs,
    };

    enum OutputIds
    {
        OUTPUT_0,
        NUM_OUTPUTS = OUTPUT_0 + n_lfos
    };

    enum LightIds
    {
        NUM_LIGHTS
    };

    modules::ModulationAssistant<QuadLFO, n_mod_params, RATE_0, n_mod_inputs, MOD_INPUT_0>
        modAssist;

    enum QuadLFOModes
    {
        INDEPENDENT,
        PHASE_OFFSET,
        RATIO,
        SPREAD
    };

    struct RateQuantity : rack::engine::ParamQuantity, modules::CalculatedName
    {
        static inline float independentRateScale(float f) { return f * 13 - 5; }
        static inline float independentRateScaleInv(float f) { return (f + 5) / 13; }
        static inline float phaseRateScale(float f) { return f; }
        static inline float phaseRateScaleInv(float f) { return f; }

        static constexpr std::array<float, 12> ratioRates{2, 3, 4, 5, 6, 7, 8, 12, 16, 24, 32};
        static inline float ratioRateScale(float f)
        {
            auto nIdx = (int)ratioRates.size();

            auto idx =
                std::clamp((int)std::round(f * ((nIdx - 1) * 2 + 1) + 0.5), 0, (nIdx - 1) * 2 + 1);
            auto res{0};
            if (idx < nIdx)
            {
                // 0 -> 11, 11 -> 0, nidx is 12, so this is just nidx - 1 - idx
                res = -ratioRates[nIdx - 1 - idx];
            }
            else if (idx == nIdx)
                res = 0;
            else
            {
                // nidx+1 -> 0
                res = ratioRates[idx - (nIdx + 1)];
            }
            return res;
        }

        QuadLFO *qlfo() { return static_cast<QuadLFO *>(module); }

        QuadLFOModes mode()
        {
            if (!module)
                return INDEPENDENT;
            auto r =
                (QuadLFOModes)(int)std::round(module->params[QuadLFO::INTERPLAY_MODE].getValue());
            return r;
        }

        void setDisplayValueString(std::string s) override
        {
            auto m = mode();
            int off = paramId - QuadLFO::RATE_0;
            auto v{0.f};
            try
            {
                v = std::stof(s);
            }
            catch (const std::invalid_argument &e)
            {
            }
            auto setRate = [this](float v) {
                if (v < 0)
                    setValue(independentRateScaleInv(0));
                else
                {
                    auto ilv = log2(v);
                    auto isv = independentRateScaleInv(ilv);
                    auto csv = std::clamp(isv, minValue, maxValue);
                    setValue(csv);
                }
            };
            switch (m)
            {
            case INDEPENDENT:
            {
                setRate(v);
            }
            break;
            case PHASE_OFFSET:
            {
                if (off == 0)
                {
                    setRate(v);
                }
                else
                {
                    auto ilv = v / 360.0;
                    auto isv = phaseRateScaleInv(ilv);
                    auto csv = std::clamp(isv, minValue, maxValue);
                    setValue(csv);
                }
            }
            break;
            case RATIO:
                if (off == 0)
                {
                    setRate(v);
                }
                else
                {
                    auto vs{1.f};
                    if (s[0] == '/')
                    {
                        vs = -1;
                        v = std::atof(s.c_str() + 1);
                    }
                    else if (s[0] == 'x')
                    {
                        v = std::atof(s.c_str() + 1);
                    }
                    if (fabs(v - 1) < 0.1)
                    {
                        setValue(0.5);
                    }
                    else
                    {
                        auto clIdx{0}, dist{100000};
                        for (auto i = 0U; i < ratioRates.size(); ++i)
                        {
                            if (fabs(v - ratioRates[i]) < dist)
                            {
                                dist = fabs(v - ratioRates[i]);
                                clIdx = i;
                            }
                        }
                        // OK now we know the closest index to what we typed so are we negative?
                        auto val{0.f};
                        if (vs > 0)
                        {
                            val = 1.f * (clIdx + ratioRates.size()) / ((ratioRates.size() - 1) * 2);
                        }
                        else
                        {
                            val = 1.f * ((ratioRates.size() - 1 - 1 - clIdx)) /
                                  ((ratioRates.size() - 1) * 2);
                        }
                        std::cout << _D(clIdx) << " " << _D(ratioRates[clIdx]) << _D(val) << _D(s)
                                  << std::endl;
                        setValue(std::clamp(val, 0.f, 1.f));
                    }
                }
                break;
            default:
                setValue(0);
            }
        }

        std::string getDisplayValueString() override
        {
            auto m = mode();
            int off = paramId - QuadLFO::RATE_0;
            auto v = getValue();
            auto fmtRate = [](auto v) {
                auto sv = independentRateScale(v);
                auto res = pow(2.0, sv);
                if (res < 10)
                    return fmt::format("{:.2f} Hz", res);
                else
                    return fmt::format("{:.1f} Hz", res);
            };
            switch (m)
            {
            case INDEPENDENT:
            {
                return fmtRate(v);
            }
            case PHASE_OFFSET:
            {
                if (off == 0)
                {
                    return fmtRate(v);
                }
                else
                {
                    return fmt::format("{:.1f}{}", v * 360.0, u8"\u00B0");
                }
            }
            case RATIO:
            {
                if (off == 0)
                {
                    return fmtRate(v);
                }
                else
                {
                    auto r = ratioRateScale(v);
                    if (r < 0)
                        return fmt::format("/{}", (int)-r);
                    else if (r > 0)
                        return fmt::format("x{}", (int)r);
                    else
                        return "x1";
                }
            }
            break;
            default:
                return std::to_string(v);
            }
        }

        std::string getLabel() override
        {
            auto res = getCalculatedName();
            return res;
        }
        std::string getCalculatedName() override
        {
            auto m = mode();
            int off = paramId - QuadLFO::RATE_0;
            switch (m)
            {
            case INDEPENDENT:
                return "Rate " + std::to_string(off + 1);
            case PHASE_OFFSET:
                if (off == 0)
                {
                    return "Rate";
                }
                else
                {
                    return "Phase Offset " + std::to_string(off + 1);
                }
            case RATIO:
                if (off == 0)
                {
                    return "Rate";
                }
                else
                {
                    return "Frequency Ratio " + std::to_string(off + 1);
                }
            default:
                return "FIXME";
            }
        }

        std::string getPanelLabel()
        {
            auto m = mode();
            int off = paramId - QuadLFO::RATE_0;
            switch (m)
            {
            case INDEPENDENT:
                return "RATE";
            case PHASE_OFFSET:
                if (off == 0)
                {
                    return "RATE";
                }
                else
                {
                    return "PHASE";
                }
            case RATIO:
                if (off == 0)
                {
                    return "RATE";
                }
                else
                {
                    return "RATIO";
                }
            default:
                return "FIXME";
            }
        }
    };

    QuadLFO() : XTModule()
    {
        {
            std::lock_guard<std::mutex> lgxt(xtSurgeCreateMutex);
            setupSurge();
        }
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

        float defaultRate0 = RateQuantity::independentRateScaleInv(0.f);
        for (int i = 0; i < n_lfos; ++i)
        {
            configParam<RateQuantity>(RATE_0 + i, 0, 1, defaultRate0);
            configParam(DEFORM_0 + i, -1, 1, 0);
            configSwitch(SHAPE_0 + i, 0, 5, 0, "Shape",
                         {"Sin", "Ramp", "Tri", "Pulse", "Rand", "S&H"});
            configSwitch(BIPOLAR_0 + i, 0, 1, 1, "Bipolar", {"Uni", "Bi"});
        }
        for (int i = 0; i < n_mod_params * n_mod_inputs; ++i)
            configParam(MOD_PARAM_0 + i, -1, 1, 0);
        for (int i = 0; i < n_mod_inputs; ++i)
            configInput(MOD_INPUT_0 + i, "Mod " + std::to_string(i));

        configSwitch(INTERPLAY_MODE, 0, 3, 0, "LFO Inter-operation Mode",
                     {"Independent LFOs", "Quad Phase", "Ratio Rates", "Spread Rate"});

        modAssist.initialize(this);
        modAssist.setupMatrix(this);
        modAssist.updateValues(this);

        for (int i = 0; i < n_lfos; ++i)
        {
            for (int c = 0; c < MAX_POLY; ++c)
            {
                processors[i][c] = std::make_unique<dsp::modulators::SimpleLFO>(storage.get());
            }
        }

        snapCalculatedNames();
    }

    std::array<std::array<std::unique_ptr<dsp::modulators::SimpleLFO>, MAX_POLY>, n_lfos>
        processors;
    void setupSurge() { setupSurgeCommon(NUM_PARAMS, false); }

    int polyChannelCount() { return nChan; }
    static int paramModulatedBy(int modIndex)
    {
        int offset = modIndex - RATE_0;
        if (offset >= n_mod_inputs * (n_mod_params + 1) || offset < 0)
            return -1;
        return offset / n_mod_inputs;
    }

    static int modulatorIndexFor(int baseParam, int modulator)
    {
        int offset = baseParam - RATE_0;
        return MOD_PARAM_0 + offset * n_mod_inputs + modulator;
    }

    float modulationDisplayValue(int paramId) override
    {
        int idx = paramId - RATE_0;
        if (idx < 0 || idx >= n_mod_params)
            return 0;

        return modAssist.modvalues[idx][0];
    }

    bool isBipolar(int paramId) override
    {
        if (paramId >= DEFORM_0 && paramId < DEFORM_0 + n_lfos)
            return true;
        return false;
    }

    void moduleSpecificSampleRateChange() override
    {
        clockProc.setSampleRate(APP->engine->getSampleRate());
    }

    std::string getRatePanelLabel(int idx)
    {
        auto pq = paramQuantities[RATE_0 + idx];
        auto rpq = dynamic_cast<RateQuantity *>(pq);
        if (!rpq)
            return "ERROR";
        return rpq->getPanelLabel();
    }

    std::string getDeformPanelLabel(int idx)
    {
        auto ip = (int)std::round(params[INTERPLAY_MODE].getValue());

        if (ip == SPREAD)
            return {"SPREAD"};
        return {"DEFORM"};
    }

    std::string getTriggerPanelLabel(int idx)
    {
        auto ip = (int)std::round(params[INTERPLAY_MODE].getValue());
        if (ip == INDEPENDENT)
            return "TRIG";

        switch (idx)
        {
        case 0:
            return "TRIG";
        case 1:
        {
            if (clockProc.clockStyle == clockProcessor_t::BPM_VOCT)
                return "BPM";
            else
                return "CLOCK";
        }
        case 2:
            return "FREEZE";
        case 3:
            return "REVERSE";
        }

        return "ERR";
    }

    typedef modules::ClockProcessor<QuadLFO> clockProcessor_t;
    clockProcessor_t clockProc;

    std::string getName() override { return std::string("QuadLFO"); }

    int nChan{-1}, chanByLFO[n_lfos]{1, 1, 1, 1};
    std::atomic<int> forcePolyphony{-1};

    int processCount{BLOCK_SIZE};

    rack::dsp::SchmittTrigger triggers[n_lfos][MAX_POLY];
    int lastInteractionType{-1};
    float uniOffset[n_lfos]{0, 0, 0, 0};
    void process(const typename rack::Module::ProcessArgs &args) override
    {
        if (inputs[CLOCK_IN].isConnected())
            clockProc.process(this, CLOCK_IN);
        else
            clockProc.disconnect(this);

        if (processCount == BLOCK_SIZE)
        {
            processCount = 0;
            auto ip = (int)std::round(params[INTERPLAY_MODE].getValue());
            if (ip != lastInteractionType)
            {
                resetDefaultsForInteractionType(ip);
            }
            lastInteractionType = ip;

            if (forcePolyphony > 0)
            {
                for (int i = 0; i < n_lfos; ++i)
                {
                    chanByLFO[i] = forcePolyphony;
                }
                nChan = forcePolyphony;
            }
            else
            {
                int cc = 1;
                if (ip == INDEPENDENT)
                {
                    for (int i = 0; i < n_lfos; ++i)
                    {
                        chanByLFO[i] = std::max(1, inputs[TRIGGER_0 + i].getChannels());
                        cc = std::max(cc, chanByLFO[i]);
                    }
                }
                else
                {
                    // share trigger 0 for poly
                    for (int i = 0; i < n_lfos; ++i)
                    {
                        chanByLFO[i] = std::max(1, inputs[TRIGGER_0].getChannels());
                        cc = std::max(cc, chanByLFO[i]);
                    }
                }
                nChan = cc;
            }

            for (int i = 0; i < n_lfos; ++i)
            {
                outputs[OUTPUT_0 + i].setChannels(chanByLFO[i]);
                uniOffset[i] = (params[BIPOLAR_0 + i].getValue() < 0.5 ? 1 : 0);
            }

            modAssist.setupMatrix(this);
            modAssist.updateValues(this);

            switch (ip)
            {
            case INDEPENDENT:
                processIndependentLFOs();
                break;
            case PHASE_OFFSET:
                processQuadPhaseLFOs();
                break;
            case RATIO:
                processRatioLFOs();
                break;
            case SPREAD:
                processSpreadLFOs();
                break;
            default:
                break;
            }
        }

        for (int i = 0; i < n_lfos; ++i)
        {
            for (int c = 0; c < chanByLFO[i]; ++c)
            {
                outputs[OUTPUT_0 + i].setVoltage(
                    (processors[i][c]->outputBlock[processCount] + uniOffset[i]) *
                        SURGE_TO_RACK_OSC_MUL,
                    c);
            }
        }
        processCount++;
    }

    void processIndependentLFOs()
    {
        for (int i = 0; i < n_lfos; ++i)
        {
            bool ic = inputs[TRIGGER_0 + i].isConnected();
            auto shape = (int)std::round(params[SHAPE_0 + i].getValue());
            auto monoTrigger = ic && inputs[TRIGGER_0 + i].getChannels() == 1;
            for (int c = 0; c < chanByLFO[i]; ++c)
            {
                auto r = RateQuantity::independentRateScale(modAssist.values[RATE_0 + i][c]);
                if (ic && triggers[i][c].process(inputs[TRIGGER_0].getVoltage(c * (!monoTrigger))))
                {
                    processors[i][c]->attack(shape);
                }
                processors[i][c]->process_block(r, modAssist.values[DEFORM_0 + i][c], shape);
            }
        }
    }

    typedef float (*RelOp)(QuadLFO *, float, int, int);
    template <RelOp R> void processQuadRelative()
    {
        bool retrig[MAX_POLY];

        // trigger is shared in this mode
        bool ic = inputs[TRIGGER_0].isConnected();
        auto monoTrigger = ic && inputs[TRIGGER_0].getChannels() == 1;

        bool fc = inputs[TRIGGER_0 + 2].isConnected();
        auto monoFreeze = fc && inputs[TRIGGER_0 + 2].getChannels() == 1;

        bool rc = inputs[TRIGGER_0 + 3].isConnected();
        auto monoRev = fc && inputs[TRIGGER_0 + 3].getChannels() == 1;

        for (int i = 0; i < n_lfos; ++i)
        {
            auto shape = (int)std::round(params[SHAPE_0 + i].getValue());
            for (int c = 0; c < chanByLFO[i]; ++c)
            {
                if (i == 0)
                {
                    retrig[c] = ic && triggers[i][c].process(
                                          inputs[TRIGGER_0].getVoltage(c * (!monoTrigger)));
                }
                bool frozen = fc && (inputs[TRIGGER_0 + 2].getVoltage(c * (!monoFreeze)) > 2);

                if (frozen) [[unlikely]]
                {
                    processors[i][c]->freeze();
                }
                else
                {
                    auto r = RateQuantity::independentRateScale(modAssist.values[RATE_0][c]);
                    if (i != 0)
                    {
                        r = R(this, r, i, c);
                    }
                    if (retrig[c])
                    {
                        processors[i][c]->attack(shape);
                    }

                    bool reverse = rc && (inputs[TRIGGER_0 + 3].getVoltage(c * (!monoRev)) > 2);

                    processors[i][c]->process_block(r, modAssist.values[DEFORM_0 + i][c], shape,
                                                    reverse);
                }
            }
        }
    }

    static float QuadPhaseRelOp(QuadLFO *that, float r, int i, int c)
    {
        auto dph = RateQuantity::phaseRateScale(that->modAssist.values[RATE_0 + i][c]);
        that->processors[i][c]->applyPhaseOffset(dph);
        return r;
    }
    void processQuadPhaseLFOs() { processQuadRelative<QuadPhaseRelOp>(); }

    static float RatioRelOp(QuadLFO *that, float r, int i, int c)
    {
        auto dph = RateQuantity::ratioRateScale(that->modAssist.basevalues[RATE_0 + i]);
        auto s = (dph < 0 ? -1 : 1);
        if (dph == 0)
            return r;

        return r + s * log2(std::fabs(dph));
    }
    void processRatioLFOs() { processQuadRelative<RatioRelOp>(); }

    void processSpreadLFOs()
    {
        // FOR NOW
        processIndependentLFOs();
    }

    void resetDefaultsForInteractionType(int ip)
    {
        for (int i = 0; i < n_lfos; ++i)
        {
            paramQuantities[RATE_0 + i]->defaultValue = RateQuantity::independentRateScale(0.5);
        }
        switch (ip)
        {
        case PHASE_OFFSET:
            for (int i = 1; i < n_lfos; ++i)
            {
                paramQuantities[RATE_0 + i]->defaultValue = 0.5;
            }
            break;

        case RATIO:
            for (int i = 1; i < n_lfos; ++i)
            {
                paramQuantities[RATE_0 + i]->defaultValue = 0.5;
            }
            break;
        default:
            break;
        }
    }

    void activateTempoSync()
    {
        std::cout << __FILE__ << ":" << __LINE__ << " " << __func__ << std::endl;
    }

    void deactivateTempoSync()
    {
        std::cout << __FILE__ << ":" << __LINE__ << " " << __func__ << std::endl;
    }

    json_t *makeModuleSpecificJson() override
    {
        auto vc = json_object();

        clockProc.toJson(vc);
        json_object_set(vc, "forcePolyphony", json_integer(forcePolyphony));

        return vc;
    }

    void readModuleSpecificJson(json_t *modJ) override
    {
        clockProc.fromJson(modJ);
        auto fp = json_object_get(modJ, "forcePolyphony");
        if (fp)
        {
            forcePolyphony = json_integer_value(fp);
        }
        else
        {
            forcePolyphony = -1;
        }
    }
};
} // namespace sst::surgext_rack::quadlfo
#endif