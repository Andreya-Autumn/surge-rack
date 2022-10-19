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

#include "Delay.h"
#include "SurgeXT.h"
#include "XTModuleWidget.h"
#include "XTWidgets.h"
#include "LayoutEngine.h"

namespace sst::surgext_rack::delay::ui
{
struct DelayWidget : widgets::XTModuleWidget
{
    typedef delay::Delay M;
    DelayWidget(M *module);

    std::array<std::array<rack::Widget *, M::n_mod_inputs>, M::n_delay_params> overlays;
    std::array<widgets::ModulatableKnob *, M::n_delay_params> underKnobs;
    std::array<widgets::ModToggleButton *, M::n_mod_inputs> toggles;

    void appendModuleSpecificMenu(rack::ui::Menu *menu) override
    {
        if (!module)
            return;
        auto xtm = static_cast<Delay *>(module);
        typedef modules::ClockProcessor<Delay> cp_t;
        menu->addChild(new rack::ui::MenuSeparator);
        auto t = xtm->clockProc.clockStyle;
        menu->addChild(
            rack::createMenuItem("Clock in QuarterNotes", CHECKMARK(t == cp_t::QUARTER_NOTE),
                                 [xtm]() { xtm->clockProc.clockStyle = cp_t::QUARTER_NOTE; }));

        menu->addChild(
            rack::createMenuItem("Clock in BPM CV", CHECKMARK(t == cp_t::BPM_VOCT),
                                 [xtm]() { xtm->clockProc.clockStyle = cp_t::BPM_VOCT; }));
    }
};

DelayWidget::DelayWidget(DelayWidget::M *module) : XTModuleWidget()
{
    setModule(module);
    typedef layout::LayoutEngine<DelayWidget, M::TIME_L, M::INPUT_CLOCK> engine_t;
    engine_t::initializeModulationToBlank(this);

    box.size = rack::Vec(rack::app::RACK_GRID_WIDTH * layout::LayoutConstants::numberOfScrews,
                         rack::app::RACK_GRID_HEIGHT);
    auto bg = new widgets::Background(box.size, "Delay", "fx", "BlankNoDisplay");
    addChild(bg);

    auto col = std::vector<float>();
    for (int i = 0; i < 4; ++i)
        col.push_back(layout::LayoutConstants::firstColumnCenter_MM +
                      layout::LayoutConstants::columnWidth_MM * i);

    const auto row1 = layout::LayoutConstants::rowStart_MM;
    const auto row2 = row1 - layout::LayoutConstants::unlabeledGap_MM;
    const auto row2S = row2 - layout::LayoutConstants::unlabeledGap_MM;
    const auto bigRow = row2 - layout::LayoutConstants::knobGap16_MM - 3;

    const auto endOfPanel = bigRow - 8;

    typedef layout::LayoutItem li_t;
    // fixme use the enums
    // clang-format off
    std::vector<li_t> layout = {
        {li_t::KNOB14, "LEFT", M::TIME_L, layout::LayoutConstants::bigCol0-2, bigRow},
        {li_t::KNOB14, "RIGHT", M::TIME_R, layout::LayoutConstants::bigCol1+2, bigRow},
        {li_t::KNOB9, "FINE", M::TIME_S, (col[2]+col[1]) * 0.5f, row2S},

        {li_t::KNOB9, "FEEDBACK", M::FEEDBACK, col[0], row2},
        {li_t::KNOB9, "XFEED", M::CROSSFEED, col[1], row2},

        {li_t::KNOB9, "", M::LOCUT, col[2], row2},
        {li_t::KNOB9, "", M::HICUT, col[3], row2},
        li_t::createKnobSpanLabel("LO - CUT - HI", col[2], row2, 2),

        {li_t::PORT, "CLOCK", M::INPUT_CLOCK, col[0], row1},

        {li_t::KNOB9, "", M::MODRATE, col[1], row1},
        {li_t::KNOB9, "", M::MODDEPTH, col[2], row1},
        li_t::createKnobSpanLabel("RATE - MOD - DEPTH", col[1], row1, 2),

        {li_t::KNOB9, "MIX", M::MIX, col[3], row1},

        li_t::createPresetLCDArea(),
    };
    // clang-format on

    for (const auto &lay : layout)
    {
        engine_t::layoutItem(this, lay, "Delay");
    }

    engine_t::addModulationSection(this, M::n_mod_inputs, M::DELAY_MOD_INPUT);
    engine_t::createInputOutputPorts(this, M::INPUT_L, M::INPUT_R, M::OUTPUT_L, M::OUTPUT_R);
    engine_t::createLeftRightInputLabels(this);

    resetStyleCouplingToModule();
}
} // namespace sst::surgext_rack::delay::ui

// namespace sst::surgext_rack::vcf::ui

rack::Model *modelSurgeDelay =
    rack::createModel<sst::surgext_rack::delay::ui::DelayWidget::M,
                      sst::surgext_rack::delay::ui::DelayWidget>("SurgeXTDelay");
