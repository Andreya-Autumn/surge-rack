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

#ifndef XTRACK_LFO_HPP
#define XTRACK_LFO_HPP

#include "SurgeXT.h"
#include "XTModule.h"
#include "rack.hpp"
#include <cstring>
#include "DebugHelpers.h"
#include "globals.h"
#include "dsp/modulators/LFOModulationSource.h"

#include <array>
#include <memory>

namespace sst::surgext_rack::lfo
{
struct LFO : modules::XTModule
{
    static constexpr int n_lfo_params{10};
    static constexpr int n_mod_inputs{4};

    static constexpr int n_steps{16};

    enum TrigTypes
    {
        DEFAULT,
        END_OF_ENV,
        END_OF_ENV_SEGMENT,
        STEP_F,
        STEP_A
    };

    enum ParamIds
    {
        RATE,
        PHASE,
        DEFORM,
        AMPLITUDE,

        E_DELAY,
        E_ATTACK,
        E_HOLD,
        E_DECAY,
        E_SUSTAIN,
        E_RELEASE,

        SHAPE,
        UNIPOLAR,

        LFO_MOD_PARAM_0,

        LFO_TYPE = LFO_MOD_PARAM_0 + n_lfo_params * n_mod_inputs,
        DEFORM_TYPE,
        WHICH_TEMPOSYNC,
        RANDOM_PHASE,
        TRIGA_TYPE,
        TRIGB_TYPE,
        STEP_SEQUENCER_STEP_0,
        STEP_SEQUENCER_TRIGGER_0 = STEP_SEQUENCER_STEP_0 + n_steps,
        STEP_SEQUENCER_START = STEP_SEQUENCER_TRIGGER_0 + n_steps,
        STEP_SEQUENCER_END,
        NUM_PARAMS
    };
    enum InputIds
    {
        INPUT_TRIGGER,
        INPUT_TRIGGER_ENVONLY,
        INPUT_CLOCK_RATE,
        INPUT_PHASE_DIRECT,
        LFO_MOD_INPUT,
        NUM_INPUTS = LFO_MOD_INPUT + n_mod_inputs,

    };
    enum OutputIds
    {
        OUTPUT_MIX,
        OUTPUT_WAVE,
        OUTPUT_ENV,
        OUTPUT_TRIGPHASE,
        OUTPUT_TRIGA,
        OUTPUT_TRIGB,
        NUM_OUTPUTS
    };
    enum LightIds
    {
        NUM_LIGHTS
    };

    LFO() : XTModule()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        setupSurge();
    }

    std::string getName() override { return "LFO"; }

    std::array<std::unique_ptr<LFOModulationSource>, MAX_POLY> surge_lfo;
    std::unique_ptr<StepSequencerStorage> surge_ss;
    std::unique_ptr<MSEGStorage> surge_ms;
    std::unique_ptr<FormulaModulatorStorage> surge_fs;
    LFOStorage *lfostorage;
    LFOStorage *lfostorageDisplay;

    modules::ModulationAssistant<LFO, n_lfo_params, RATE, n_mod_inputs, LFO_MOD_INPUT> modAssist;

    rack::simd::float_4 output0[3][4], output1[3][4];

    std::map<int, size_t> paramOffsetByID;

    void setupSurge()
    {
        setupSurgeCommon(NUM_PARAMS, false);
        for (int i = 0; i < MAX_POLY; ++i)
            surge_lfo[i] = std::make_unique<LFOModulationSource>();

        surge_ss = std::make_unique<StepSequencerStorage>();
        surge_ss->loop_start = 0;
        surge_ss->loop_end = n_steps - 1;
        surge_ms = std::make_unique<MSEGStorage>();
        surge_fs = std::make_unique<FormulaModulatorStorage>();

        lfostorage = &(storage->getPatch().scene[0].lfo[0]);
        lfostorage->delay.deactivated = false;
        lfostorage->trigmode.val.i = lm_keytrigger;

        lfostorageDisplay = &(storage->getPatch().scene[0].lfo[1]);
        lfostorageDisplay->delay.deactivated = false;
        lfostorageDisplay->trigmode.val.i = lm_keytrigger;

        paramOffsetByID[RATE] = 0;
        paramOffsetByID[SHAPE] = 1;
        paramOffsetByID[PHASE] = 2;
        paramOffsetByID[AMPLITUDE] = 3;
        paramOffsetByID[DEFORM] = 4;
        // skip trigmode
        paramOffsetByID[UNIPOLAR] = 6;
        paramOffsetByID[E_DELAY] = 7;
        paramOffsetByID[E_HOLD] = 8; // yeah the storage object is mis-ordered
        paramOffsetByID[E_ATTACK] = 9;
        paramOffsetByID[E_DECAY] = 10;
        paramOffsetByID[E_SUSTAIN] = 11;
        paramOffsetByID[E_RELEASE] = 12;

        auto *par0 = &(lfostorage->rate);

        for (int p = RATE; p < LFO_MOD_PARAM_0; ++p)
        {
            auto *par = &par0[paramOffsetByID[p]];
            configParam<modules::SurgeParameterParamQuantity>(p, 0, 1, par->get_default_value_f01(),
                                                              par->get_name());
        }

        for (int p = LFO_MOD_PARAM_0; p < LFO_TYPE; ++p)
        {
            auto name =
                std::string("Mod ") + std::to_string((p - LFO_MOD_PARAM_0) % n_mod_inputs + 1);
            configParam<modules::SurgeParameterModulationQuantity>(p, -1, 1, 0, name);
        }

        configParam(DEFORM_TYPE, 0, 4, 0, "Deform Type");
        configParam(WHICH_TEMPOSYNC, 0, 3, 1, "Which Temposync");
        configParam(RANDOM_PHASE, 0, 1, 0, "Randomize Iniital Phase");

        configParam(TRIGA_TYPE, 0, 3, DEFAULT, "Trigger A");
        configParam(TRIGB_TYPE, 0, 3, DEFAULT, "Trigger B");

        for (int i = 0; i < n_steps; ++i)
        {
            configParam(STEP_SEQUENCER_STEP_0 + i, -1, 1, 0,
                        std::string("Step ") + std::to_string(i + 1));
            params[STEP_SEQUENCER_STEP_0 + i].setValue(1.f * i / (n_steps - 1));
            configParam(STEP_SEQUENCER_TRIGGER_0 + i, 0, 3, 0,
                        std::string("Step Trigger ") + std::to_string(i + 1));
        }
        configParam(STEP_SEQUENCER_START, 0, n_steps - 1, 0, "First Loop Point")->snapEnabled =
            true;
        configParam(STEP_SEQUENCER_END, 1, n_steps, n_steps, "Last Loop Point")->snapEnabled = true;

        for (int i = 0; i < MAX_POLY; ++i)
        {
            surge_lfo[i]->assign(storage.get(), lfostorage, storage->getPatch().scenedata[0],
                                 nullptr, surge_ss.get(), surge_ms.get(), surge_fs.get());
            isGated[i] = false;
            isGateConnected[i] = false;
            isTriggered[i] = false;
            isTriggeredEnvOnly[i] = false;
            priorIntPhase[i] = -1;
            endPhaseCountdown[i] = 0;
            trigABCountdown[0][i] = 0;
            trigABCountdown[1][i] = 0;
        }
        setupStorageRanges(&(lfostorage->rate), &(lfostorage->release));

        for (int s = 0; s < 3; ++s)
        {
            for (int i = 0; i < 4; ++i)
            {
                output0[s][i] = rack::simd::float_4::zero();
                output1[s][i] = rack::simd::float_4::zero();
            }
        }

        configInput(INPUT_TRIGGER, "Retrigger Full LFO");
        configInput(INPUT_TRIGGER_ENVONLY, "Retrigger Envelope Only");
        configInput(INPUT_CLOCK_RATE, "Clock");
        configInput(INPUT_PHASE_DIRECT, "Direct Phase (0-10v for 0-1 Phase)");
        for (int i = 0; i < n_mod_inputs; ++i)
            configInput(LFO_MOD_INPUT + i, std::string("Mod ") + std::to_string(i + 1));

        configOutput(OUTPUT_MIX, "LFO Wave x Envelope Generator");
        configOutput(OUTPUT_WAVE, "LFO Wave");
        configOutput(OUTPUT_ENV, "Envelope Generator");
        configOutput(OUTPUT_TRIGPHASE, "Trigger on End of Wave Cycle");
        configOutput(OUTPUT_TRIGA, "Trigger on end of Envelope Stage or Step Trigger A");
        configOutput(OUTPUT_TRIGB, "Trigger on end of Envelope or Step Trigger B");

        modAssist.initialize(this);
    }

    Parameter *surgeDisplayParameterForParamId(int paramId) override
    {
        if (paramOffsetByID.find(paramId) == paramOffsetByID.end())
        {
            std::cout << "ERROR: NOT FOUND PARAM ID " << paramId << std::endl;
            return nullptr;
        }

        auto po = paramOffsetByID[paramId];
        auto *par0 = &(lfostorage->rate);
        auto *par = &par0[po];
        return par;
    }

    Parameter *surgeDisplayParameterForModulatorParamId(int modParamId) override
    {
        auto paramId = paramModulatedBy(modParamId);
        if (paramId < RATE || paramId >= RATE + n_lfo_params)
            return nullptr;

        auto po = paramOffsetByID[paramId];
        auto *par0 = &(lfostorage->rate);
        auto *par = &par0[po];
        return par;
    }

    static int paramModulatedBy(int modIndex)
    {
        int offset = modIndex - LFO_MOD_PARAM_0;
        if (offset >= n_mod_inputs * (n_lfo_params + 1) || offset < 0)
            return -1;
        return offset / n_mod_inputs;
    }

    float modulationDisplayValue(int paramId) override
    {
        int idx = paramId - RATE;
        if (idx < 0 || idx >= n_lfo_params + 1)
            return 0;
        return modAssist.animValues[idx];
    }

    static int modulatorIndexFor(int baseParam, int modulator)
    {
        int offset = baseParam - RATE;
        return LFO_MOD_PARAM_0 + offset * n_mod_inputs + modulator;
    }

    int polyChannelCount() { return std::max(1, lastNChan); }

    int lastStep = BLOCK_SIZE;
    int lastNChan = -1;
    bool firstProcess{true};

    rack::dsp::SchmittTrigger envGateTrigger[MAX_POLY], envGateTriggerEnvOnly[MAX_POLY];
    bool isGated[MAX_POLY], isGateConnected[MAX_POLY];
    bool isTriggered[MAX_POLY], isTriggeredEnvOnly[MAX_POLY];
    int priorIntPhase[MAX_POLY], endPhaseCountdown[MAX_POLY], priorEnvStage[MAX_POLY];
    int trigABCountdown[2][MAX_POLY];

    modules::ClockProcessor<LFO> clockProc;

    void activateTempoSync()
    {
        auto wts = (int)std::round(paramQuantities[LFO::WHICH_TEMPOSYNC]->getValue());
        auto wtR = (bool)(wts & 1);
        auto wtE = (bool)(wts & 2);

        for (auto ls : {lfostorage, lfostorageDisplay})
        {
            ls->rate.temposync = wtR;
            auto *par0 = &(ls->rate);

            for (int p = E_DELAY; p < LFO_MOD_PARAM_0; ++p)
            {
                auto *par = &par0[paramOffsetByID[p]];
                if (par->can_temposync())
                    par->temposync = wtE;
            }
        }
    }
    void deactivateTempoSync()
    {
        for (auto ls : {lfostorage, lfostorageDisplay})
        {
            auto *par0 = &(ls->rate);
            for (int p = RATE; p < LFO_MOD_PARAM_0; ++p)
            {
                auto *par = &par0[paramOffsetByID[p]];
                if (par->can_temposync())
                    par->temposync = false;
            }
        }
    }

    void process(const typename rack::Module::ProcessArgs &args) override
    {
        int nChan = std::max(1, inputs[INPUT_TRIGGER].getChannels());
        nChan = std::max(nChan, inputs[INPUT_TRIGGER_ENVONLY].getChannels());
        outputs[OUTPUT_MIX].setChannels(nChan);
        outputs[OUTPUT_ENV].setChannels(nChan);
        outputs[OUTPUT_WAVE].setChannels(nChan);
        if (nChan != lastNChan)
        {
            firstProcess = true;
            lastNChan = nChan;
            for (int i = nChan; i < MAX_POLY; ++i)
                lastStep = BLOCK_SIZE;
        }

        for (int c = 0; c < nChan; ++c)
        {
            if (inputs[INPUT_TRIGGER].isConnected() &&
                envGateTrigger[c].process(inputs[INPUT_TRIGGER].getVoltage(c)))
                isTriggered[c] = true;
            if (inputs[INPUT_TRIGGER_ENVONLY].isConnected() &&
                envGateTriggerEnvOnly[c].process(inputs[INPUT_TRIGGER_ENVONLY].getVoltage(c)))
                isTriggeredEnvOnly[c] = true;
        }

        if (inputs[INPUT_CLOCK_RATE].isConnected())
            clockProc.process(this, INPUT_CLOCK_RATE);
        else
            clockProc.disconnect(this);

        if (lastStep == BLOCK_SIZE)
            lastStep = 0;

        if (lastStep == 0)
        {
            modAssist.setupMatrix(this);
            modAssist.updateValues(this);

            auto direct = inputs[INPUT_PHASE_DIRECT].isConnected();

            for (int s = 0; s < 3; ++s)
                for (int i = 0; i < 4; ++i)
                    output0[s][i] = output1[s][i];

            auto tat = (TrigTypes)std::round(params[TRIGA_TYPE].getValue());
            auto tbt = (TrigTypes)std::round(params[TRIGB_TYPE].getValue());

            if (lfostorageDisplay->shape.val.i == lt_stepseq)
            {
                tat = (tat == DEFAULT) ? STEP_F : tat;
                tbt = (tbt == DEFAULT) ? STEP_A : tbt;
            }
            else
            {
                tat = END_OF_ENV_SEGMENT;
                tbt = END_OF_ENV;
            }

            float ts[3][16];
            // setup the base case ready for modulation below
            for (auto ls : {lfostorageDisplay, lfostorage})
            {
                // Setup the display storage
                auto *par0 = &(ls->rate);
                for (int p = RATE; p < LFO_MOD_PARAM_0; ++p)
                {
                    auto *par = &par0[paramOffsetByID[p]];
                    par->set_value_f01(params[p].getValue());
                }

                auto dt = (int)std::round(params[LFO::DEFORM_TYPE].getValue());
                auto nd = lt_num_deforms[lfostorageDisplay->shape.val.i];
                auto dto = std::clamp(dt, 0, nd ? (nd - 1) : 0);
                if (dto != dt)
                {
                    params[LFO::DEFORM_TYPE].setValue(dto);
                }
                ls->deform.deform_type = dto;
            }

            if (lfostorageDisplay->shape.val.i == lt_stepseq)
            {
                surge_ss->trigmask = 0;
                for (int i = 0; i < n_steps; ++i)
                {
                    surge_ss->steps[i] = params[STEP_SEQUENCER_STEP_0 + i].getValue();
                    auto ltm = (int)std::round(params[STEP_SEQUENCER_TRIGGER_0 + i].getValue());
                    if (ltm & 1)
                        surge_ss->trigmask |= (UINT64_C(1) << (16 + i));
                    if (ltm & 2)
                        surge_ss->trigmask |= (UINT64_C(1) << (32 + i));
                }
                surge_ss->loop_start = (int)std::round(params[STEP_SEQUENCER_START].getValue());
                surge_ss->loop_end = (int)std::round(params[STEP_SEQUENCER_END].getValue()) - 1;
            }

            lfostorage->rate.deactivated = direct;
            for (int c = 0; c < nChan; ++c)
            {
                bool inNewAttack = firstProcess;
                bool inNewEnvAttack = false;
                // move this to every sample and record it eliminating the first process thing too
                if (isTriggered[c])
                {
                    isGated[c] = true;
                    inNewAttack = true;
                    isGateConnected[c] = true;
                    isTriggered[c] = false;
                }
                else if (isTriggeredEnvOnly[c])
                {
                    inNewEnvAttack = true;
                    isTriggeredEnvOnly[c] = false;
                    isGated[c] = true;
                    isGateConnected[c] = true;
                }
                else if (inputs[INPUT_TRIGGER].isConnected() && isGated[c] &&
                         (inputs[INPUT_TRIGGER].getVoltage(c) < 1.f))
                {
                    isGated[c] = false;
                    surge_lfo[c]->release();
                    isGateConnected[c] = true;
                }
                else if (inputs[INPUT_TRIGGER_ENVONLY].isConnected() && isGated[c] &&
                         (inputs[INPUT_TRIGGER_ENVONLY].getVoltage(c) < 1.f))
                {
                    isGated[c] = false;
                    surge_lfo[c]->release();

                    isGateConnected[c] = true;
                }
                else if (!inputs[INPUT_TRIGGER].isConnected() &&
                         !inputs[INPUT_TRIGGER_ENVONLY].isConnected())
                {
                    if (isGateConnected[c])
                        inNewAttack = true;
                    if (!isGated[c])
                        inNewAttack = true;
                    isGated[c] = true;
                    isGateConnected[c] = false;
                }

                if (direct)
                {
                    auto pd = inputs[INPUT_PHASE_DIRECT].getVoltage(c) * RACK_TO_SURGE_CV_MUL;
                    lfostorage->start_phase.set_value_f01(pd);
                }
                lfostorage->trigmode.val.i =
                    params[RANDOM_PHASE].getValue() > 0.5 ? lm_random : lm_keytrigger;

                copyScenedataSubset(0, storage_id_start, storage_id_end);

                // Apply the modulation to the copied result to preserve temposync
                auto *par0 = &(lfostorage->rate);
                auto &pt = storage->getPatch().scenedata[0];
                for (int p = RATE; p < RATE + n_lfo_params; ++p)
                {
                    auto *oap = &par0[paramOffsetByID[p]];
                    if (oap->valtype == vt_float)
                    {
                        pt[oap->param_id_in_scene].f +=
                            modAssist.modvalues[p - RATE][c] * (oap->val_max.f - oap->val_min.f);
                    }
                }

                if (inNewAttack)
                {
                    surge_lfo[c]->attack();
                    priorIntPhase[c] = -1;
                    priorEnvStage[c] = -1;
                }
                surge_lfo[c]->process_block();
                if (inNewAttack)
                {
                    // Do the painful thing in the infrequent case
                    output0[0][c / 4].s[c % 4] = surge_lfo[c]->get_output(0);
                    output0[1][c / 4].s[c % 4] = surge_lfo[c]->get_output(1);
                    output0[2][c / 4].s[c % 4] = surge_lfo[c]->get_output(2);
                    surge_lfo[c]->process_block();
                }
                else if (inNewEnvAttack)
                {
                    surge_lfo[c]->retriggerEnvelope();
                }
                // repeat for env stage also
                if (surge_lfo[c]->getIntPhase() != priorIntPhase[c])
                {
                    priorIntPhase[c] = surge_lfo[c]->getIntPhase();
                    endPhaseCountdown[c] = pulseSamples;
                }
                bool newEnvStage{false}, newEnvRelease{false};
                if (surge_lfo[c]->getEnvState() != priorEnvStage[c])
                {
                    priorEnvStage[c] = surge_lfo[c]->getEnvState();
                    newEnvStage = true;
                    if (priorEnvStage[c] == lfoeg_stuck)
                        newEnvRelease = !isGated[c];
                }

                auto feg = surge_lfo[c]->retrigger_FEG;
                auto aeg = surge_lfo[c]->retrigger_AEG;
                surge_lfo[c]->retrigger_FEG = false;
                surge_lfo[c]->retrigger_AEG = false;

                for (int idx = 0; idx < 2; ++idx)
                {
                    auto ta = idx == 0 ? tat : tbt;
                    if (trigABCountdown[idx][c] == 0)
                    {
                        switch (ta)
                        {
                        case END_OF_ENV:
                            trigABCountdown[idx][c] = newEnvRelease ? pulseSamples : 0;
                            break;
                        case END_OF_ENV_SEGMENT:
                            trigABCountdown[idx][c] = newEnvStage ? pulseSamples : 0;
                            break;
                        case STEP_F:
                            trigABCountdown[idx][c] = feg ? pulseSamples : 0;
                            break;
                        case STEP_A:
                            trigABCountdown[idx][c] = aeg ? pulseSamples : 0;
                            break;
                        case DEFAULT:
                            break;
                        }
                    }
                }

                for (int p = 0; p < 3; ++p)
                    ts[p][c] = surge_lfo[c]->get_output(p);
            }
            for (int p = 0; p < 3; ++p)
                for (int i = 0; i < 4; ++i)
                    output1[p][i] = rack::simd::float_4::load(&ts[p][i * 4]);

            firstProcess = false;
        }

        float frac = 1.0 * lastStep / BLOCK_SIZE;
        lastStep++;
        auto mul = SURGE_TO_RACK_OSC_MUL;
        if (lfostorage->unipolar.val.b)
            mul = SURGE_TO_RACK_CV_MUL;
        for (int c = 0; c < nChan; c += 4)
        {
            for (int p = 0; p < 3; ++p)
            {
                rack::simd::float_4 outputI =
                    (output0[p][c / 4] * (1.0 - frac) + output1[p][c / 4] * frac) * mul;
                outputI.store(outputs[OUTPUT_MIX + p].getVoltages(c));
            }
        }

        for (int c = 0; c < nChan; ++c)
        {
            if (endPhaseCountdown[c] > 0)
            {
                endPhaseCountdown[c]--;
                outputs[OUTPUT_TRIGPHASE].setVoltage(10.0, c);
            }
            else
                outputs[OUTPUT_TRIGPHASE].setVoltage(0.0, c);

            for (int ab = 0; ab < 2; ++ab)
            {
                if (trigABCountdown[ab][c] > 0)
                {
                    trigABCountdown[ab][c]--;
                    outputs[OUTPUT_TRIGA + ab].setVoltage(10.0, c);
                }
                else
                {
                    outputs[OUTPUT_TRIGA + ab].setVoltage(0, c);
                }
            }
        }
    }

    int pulseSamples{48000 / 100};
    void moduleSpecificSampleRateChange() override
    {
        clockProc.setSampleRate(APP->engine->getSampleRate());
        // rack spec says triggers are at least 10ms so make it 12 to be safe
        pulseSamples = (int)std::ceil(APP->engine->getSampleRate() * 0.012);
    }

    json_t *makeModuleSpecificJson() override
    {
        auto fx = json_object();
        json_object_set(fx, "clockStyle", json_integer((int)clockProc.clockStyle));
        return fx;
    }

    void readModuleSpecificJson(json_t *modJ) override
    {
        auto cs = json_object_get(modJ, "clockStyle");
        if (cs)
        {
            auto csv = json_integer_value(cs);
            clockProc.clockStyle =
                static_cast<typename modules::ClockProcessor<LFO>::ClockStyle>(csv);
        }
    }

    void setWhichTemposyc(bool doRate, bool doEnv)
    {
        auto val = (doRate ? 1 : 0) | (doEnv ? 2 : 0);
        paramQuantities[WHICH_TEMPOSYNC]->setValue(val);
        if (inputs[INPUT_CLOCK_RATE].isConnected())
            activateTempoSync();
    }
};
} // namespace sst::surgext_rack::lfo
#endif