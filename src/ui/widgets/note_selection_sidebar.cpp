#include "eatsbits/ui/widgets/note_selection_sidebar.hpp"
#include "eatsbits/eatscript/evaluator.hpp"
#include <algorithm>

namespace eatsbits::ui {

NoteInspectorRegistry& NoteInspectorRegistry::instance() {
    static NoteInspectorRegistry s_instance;
    return s_instance;
}

NoteInspectorRegistry::NoteInspectorRegistry() {
    resetToDefaults();
}

void NoteInspectorRegistry::clear() {
    sections_.clear();
}

void NoteInspectorRegistry::resetToDefaults() {
    sections_.clear();

    // 1. Pitch Transposition Section
    {
        InspectorSection pitchSec;
        pitchSec.id = "pitch";
        pitchSec.title = "PITCH TRANSPOSE";

        InspectorRow row;
        row.rowHeight = 22.0f;
        row.marginBottom = 12.0f;

        InspectorItem bOctDown{"p_oct_down", "-12", InspectorItemType::Button, SelectionSidebarAction::Transpose};
        bOctDown.intParam = -12;
        bOctDown.width = 34.0f;
        row.items.push_back(bOctDown);

        InspectorItem bSemiDown{"p_semi_down", "-1", InspectorItemType::Button, SelectionSidebarAction::Transpose};
        bSemiDown.intParam = -1;
        bSemiDown.width = 34.0f;
        row.items.push_back(bSemiDown);

        InspectorItem bBadge{"p_badge", "+- PITCH", InspectorItemType::Badge, SelectionSidebarAction::None};
        bBadge.width = 68.0f;
        row.items.push_back(bBadge);

        InspectorItem bSemiUp{"p_semi_up", "+1", InspectorItemType::Button, SelectionSidebarAction::Transpose};
        bSemiUp.intParam = 1;
        bSemiUp.width = 34.0f;
        row.items.push_back(bSemiUp);

        InspectorItem bOctUp{"p_oct_up", "+12", InspectorItemType::Button, SelectionSidebarAction::Transpose};
        bOctUp.intParam = 12;
        bOctUp.width = 34.0f;
        row.items.push_back(bOctUp);

        pitchSec.rows.push_back(row);
        sections_.push_back(pitchSec);
    }

    // 2. Position & Duration Section
    {
        InspectorSection posSec;
        posSec.id = "position_duration";
        posSec.title = "POSITION & DURATION";

        InspectorRow row1;
        row1.rowHeight = 22.0f;
        row1.marginBottom = 6.0f;

        InspectorItem nLeft{"nudge_left", "-STEP", InspectorItemType::Button, SelectionSidebarAction::Nudge};
        nLeft.floatParam = -1.0f;
        nLeft.width = 52.0f;
        row1.items.push_back(nLeft);

        InspectorItem nRight{"nudge_right", "+STEP", InspectorItemType::Button, SelectionSidebarAction::Nudge};
        nRight.floatParam = 1.0f;
        nRight.width = 52.0f;
        row1.items.push_back(nRight);

        InspectorItem dMinus{"dur_minus", "-LEN", InspectorItemType::Button, SelectionSidebarAction::ChangeDuration};
        dMinus.floatParam = -0.25f;
        dMinus.width = 48.0f;
        row1.items.push_back(dMinus);

        InspectorItem dPlus{"dur_plus", "+LEN", InspectorItemType::Button, SelectionSidebarAction::ChangeDuration};
        dPlus.floatParam = 0.25f;
        dPlus.width = 48.0f;
        row1.items.push_back(dPlus);

        posSec.rows.push_back(row1);

        InspectorRow row2;
        row2.rowHeight = 20.0f;
        row2.marginBottom = 12.0f;

        InspectorItem c25{"dur_preset_25", "0.25", InspectorItemType::Chip, SelectionSidebarAction::SetDuration};
        c25.floatParam = 0.25f;
        c25.width = 52.0f;
        row2.items.push_back(c25);

        InspectorItem c50{"dur_preset_50", "0.50", InspectorItemType::Chip, SelectionSidebarAction::SetDuration};
        c50.floatParam = 0.50f;
        c50.width = 52.0f;
        row2.items.push_back(c50);

        InspectorItem c75{"dur_preset_75", "0.75", InspectorItemType::Chip, SelectionSidebarAction::SetDuration};
        c75.floatParam = 0.75f;
        c75.width = 52.0f;
        row2.items.push_back(c75);

        InspectorItem c100{"dur_preset_100", "1.00", InspectorItemType::Chip, SelectionSidebarAction::SetDuration};
        c100.floatParam = 1.00f;
        c100.width = 52.0f;
        row2.items.push_back(c100);

        posSec.rows.push_back(row2);
        sections_.push_back(posSec);
    }

    // 3. Velocity Presets Section
    {
        InspectorSection velSec;
        velSec.id = "velocity";
        velSec.title = "VELOCITY PRESETS";

        InspectorRow row;
        row.rowHeight = 22.0f;
        row.marginBottom = 12.0f;

        InspectorItem v25{"v_25", "25%", InspectorItemType::Chip, SelectionSidebarAction::SetVelocity};
        v25.floatParam = 0.25f;
        v25.width = 40.0f;
        row.items.push_back(v25);

        InspectorItem v50{"v_50", "50%", InspectorItemType::Chip, SelectionSidebarAction::SetVelocity};
        v50.floatParam = 0.50f;
        v50.width = 40.0f;
        row.items.push_back(v50);

        InspectorItem v75{"v_75", "75%", InspectorItemType::Chip, SelectionSidebarAction::SetVelocity};
        v75.floatParam = 0.75f;
        v75.width = 40.0f;
        row.items.push_back(v75);

        InspectorItem v100{"v_100", "100%", InspectorItemType::Chip, SelectionSidebarAction::SetVelocity};
        v100.floatParam = 1.00f;
        v100.width = 40.0f;
        row.items.push_back(v100);

        InspectorItem vHum{"v_hum", "HUMAN", InspectorItemType::Button, SelectionSidebarAction::Humanize};
        vHum.floatParam = 0.15f;
        vHum.width = 52.0f;
        row.items.push_back(vHum);

        velSec.rows.push_back(row);
        sections_.push_back(velSec);
    }

    // 4. Expression & Articulation Section
    {
        InspectorSection exprSec;
        exprSec.id = "expression";
        exprSec.title = "EXPRESSION / TB-303";

        InspectorRow row1;
        row1.rowHeight = 26.0f;
        row1.marginBottom = 8.0f;

        InspectorItem tReg{"t_regular", "REGULAR", InspectorItemType::Toggle, SelectionSidebarAction::SetSlide};
        tReg.intParam = 0;
        tReg.width = 108.0f;
        row1.items.push_back(tReg);

        InspectorItem tGliss{"t_gliss", "GLISS / BEND", InspectorItemType::Toggle, SelectionSidebarAction::SetSlide};
        tGliss.intParam = 1;
        tGliss.width = 108.0f;
        row1.items.push_back(tGliss);

        exprSec.rows.push_back(row1);

        InspectorRow row2;
        row2.rowHeight = 20.0f;
        row2.marginBottom = 4.0f;

        const char* arts1[] = {"NORMAL", "PIZZ", "STACC"};
        for (const char* art : arts1) {
            InspectorItem cArt{std::string("art_") + art, art, InspectorItemType::Chip, SelectionSidebarAction::SetArticulation};
            cArt.stringParam = art;
            cArt.width = 36.0f;
            row2.items.push_back(cArt);
        }
        exprSec.rows.push_back(row2);

        InspectorRow row3;
        row3.rowHeight = 20.0f;
        row3.marginBottom = 12.0f;

        const char* arts2[] = {"LEGATO", "SLAP", "FLAM"};
        for (const char* art : arts2) {
            InspectorItem cArt{std::string("art_") + art, art, InspectorItemType::Chip, SelectionSidebarAction::SetArticulation};
            cArt.stringParam = art;
            cArt.width = 36.0f;
            row3.items.push_back(cArt);
        }
        exprSec.rows.push_back(row3);

        sections_.push_back(exprSec);
    }

    // 5. Selection Utilities Section
    {
        InspectorSection utilSec;
        utilSec.id = "utilities";
        utilSec.title = "SELECTION UTILITIES";

        InspectorRow row1;
        row1.rowHeight = 24.0f;
        row1.marginBottom = 4.0f;

        InspectorItem qBtn{"u_quantize", "QUANTIZE", InspectorItemType::Button, SelectionSidebarAction::Quantize};
        qBtn.floatParam = 1.0f;
        qBtn.width = 110.0f;
        row1.items.push_back(qBtn);

        InspectorItem bBend{"u_set_bend", "SET ALL BEND", InspectorItemType::Button, SelectionSidebarAction::SetSlide};
        bBend.intParam = 1;
        bBend.width = 108.0f;
        row1.items.push_back(bBend);

        utilSec.rows.push_back(row1);

        InspectorRow row2;
        row2.rowHeight = 24.0f;
        row2.marginBottom = 4.0f;

        InspectorItem selAll{"u_sel_all", "SELECT ALL", InspectorItemType::Button, SelectionSidebarAction::SelectAll};
        selAll.width = 110.0f;
        row2.items.push_back(selAll);

        InspectorItem inv{"u_invert", "INVERT", InspectorItemType::Button, SelectionSidebarAction::Invert};
        inv.width = 108.0f;
        row2.items.push_back(inv);

        utilSec.rows.push_back(row2);

        InspectorRow row3;
        row3.rowHeight = 24.0f;
        row3.marginBottom = 8.0f;

        InspectorItem retro{"u_retrograde", "RETROGRADE", InspectorItemType::Button, SelectionSidebarAction::Retrograde};
        retro.width = 110.0f;
        row3.items.push_back(retro);

        InspectorItem invP{"u_invert_pitch", "INVERT PITCH", InspectorItemType::Button, SelectionSidebarAction::InvertPitch};
        invP.width = 108.0f;
        row3.items.push_back(invP);

        utilSec.rows.push_back(row3);

        sections_.push_back(utilSec);
    }
}

void NoteInspectorRegistry::addSection(const InspectorSection& section) {
    auto it = std::find_if(sections_.begin(), sections_.end(), [&](const InspectorSection& s) {
        return s.id == section.id;
    });
    if (it != sections_.end()) {
        *it = section;
    } else {
        sections_.push_back(section);
    }
}

bool NoteInspectorRegistry::removeSection(const std::string& sectionId) {
    auto it = std::remove_if(sections_.begin(), sections_.end(), [&](const InspectorSection& s) {
        return s.id == sectionId || s.title == sectionId;
    });
    if (it != sections_.end()) {
        sections_.erase(it, sections_.end());
        return true;
    }
    return false;
}

InspectorSection* NoteInspectorRegistry::findSection(const std::string& sectionId) {
    for (auto& s : sections_) {
        if (s.id == sectionId || s.title == sectionId) return &s;
    }
    return nullptr;
}

void NoteInspectorRegistry::addCustomButton(const std::string& sectionTitle,
                                            const std::string& label,
                                            const std::string& scriptCode,
                                            float width) {
    InspectorSection* sec = findSection(sectionTitle);
    if (!sec) {
        InspectorSection newSec;
        newSec.id = sectionTitle;
        newSec.title = sectionTitle;
        sections_.push_back(newSec);
        sec = &sections_.back();
    }

    InspectorItem item;
    item.id = sectionTitle + "_" + label;
    item.label = label;
    item.type = InspectorItemType::Button;
    item.action = SelectionSidebarAction::Script;
    item.scriptCode = scriptCode;
    item.width = (width > 0.0f) ? width : 108.0f;
    item.height = 24.0f;

    if (sec->rows.empty() || sec->rows.back().items.size() >= 2) {
        InspectorRow newRow;
        newRow.rowHeight = 24.0f;
        newRow.marginBottom = 4.0f;
        newRow.items.push_back(item);
        sec->rows.push_back(newRow);
    } else {
        sec->rows.back().items.push_back(item);
    }
}

void NoteInspectorRegistry::registerEatscriptApi(eatscript::Evaluator& evaluator) {
    using namespace eatsbits::eatscript;

    evaluator.registerMemberFunction("eat.inspector", "reset_defaults", [this](const auto&, const auto&) -> Value {
        resetToDefaults();
        return Value(true);
    });

    evaluator.registerMemberFunction("eat.inspector", "clear", [this](const auto&, const auto&) -> Value {
        clear();
        return Value(true);
    });

    evaluator.registerMemberFunction("eat.inspector", "remove_section", [this](const auto& args, const auto& kw) -> Value {
        std::string sid = resolveArg(args, kw, 0, "id", Value("")).asString();
        return Value(removeSection(sid));
    });

    evaluator.registerMemberFunction("eat.inspector", "add_button", [this](const auto& args, const auto& kw) -> Value {
        std::string secTitle = resolveArg(args, kw, 0, "section", Value("CUSTOM")).asString();
        std::string label = resolveArg(args, kw, 1, "label", Value("ACTION")).asString();
        std::string action = resolveArg(args, kw, 2, "action", Value("script")).asString();
        float val = resolveArg(args, kw, 3, "val", Value(0.0)).asFloat(0.0f);
        float w = resolveArg(args, kw, 4, "width", Value(0.0)).asFloat(0.0f);

        InspectorSection* sec = findSection(secTitle);
        if (!sec) {
            InspectorSection newSec;
            newSec.id = secTitle;
            newSec.title = secTitle;
            sections_.push_back(newSec);
            sec = &sections_.back();
        }

        InspectorItem item;
        item.id = secTitle + "_" + label;
        item.label = label;
        item.type = InspectorItemType::Button;
        item.width = (w > 0.0f) ? w : 108.0f;
        item.height = 24.0f;

        if (action == "transpose") {
            item.action = SelectionSidebarAction::Transpose;
            item.intParam = static_cast<int>(val);
        } else if (action == "nudge") {
            item.action = SelectionSidebarAction::Nudge;
            item.floatParam = val;
        } else if (action == "duration" || action == "set_duration") {
            item.action = SelectionSidebarAction::SetDuration;
            item.floatParam = val;
        } else if (action == "velocity" || action == "set_velocity") {
            item.action = SelectionSidebarAction::SetVelocity;
            item.floatParam = val;
        } else if (action == "humanize") {
            item.action = SelectionSidebarAction::Humanize;
            item.floatParam = val > 0.0f ? val : 0.15f;
        } else if (action == "quantize") {
            item.action = SelectionSidebarAction::Quantize;
            item.floatParam = val > 0.0f ? val : 1.0f;
        } else if (action == "retrograde") {
            item.action = SelectionSidebarAction::Retrograde;
        } else if (action == "invert_pitch") {
            item.action = SelectionSidebarAction::InvertPitch;
        } else {
            item.action = SelectionSidebarAction::Script;
            item.scriptCode = action;
        }

        if (sec->rows.empty() || sec->rows.back().items.size() >= 2) {
            InspectorRow newRow;
            newRow.rowHeight = 24.0f;
            newRow.marginBottom = 4.0f;
            newRow.items.push_back(item);
            sec->rows.push_back(newRow);
        } else {
            sec->rows.back().items.push_back(item);
        }

        return Value(true);
    });

    evaluator.registerMemberFunction("eat.inspector", "add_script_button", [this](const auto& args, const auto& kw) -> Value {
        std::string secTitle = resolveArg(args, kw, 0, "section", Value("CUSTOM")).asString();
        std::string label = resolveArg(args, kw, 1, "label", Value("SCRIPT")).asString();
        std::string script = resolveArg(args, kw, 2, "script", Value("")).asString();
        float w = resolveArg(args, kw, 3, "width", Value(0.0)).asFloat(0.0f);

        addCustomButton(secTitle, label, script, w);
        return Value(true);
    });

    evaluator.registerMemberFunction("eat.inspector", "add_section", [this](const auto& args, const auto& kw) -> Value {
        std::string title = resolveArg(args, kw, 0, "title", Value("SECTION")).asString();
        Value itemsVal = resolveArg(args, kw, 1, "items", Value(std::vector<Value>{}));

        InspectorSection sec;
        sec.id = title;
        sec.title = title;

        if (itemsVal.isList()) {
            InspectorRow currentRow;
            currentRow.rowHeight = 24.0f;
            currentRow.marginBottom = 4.0f;

            for (const auto& itemDict : itemsVal.listVal) {
                if (!itemDict.isDict()) continue;

                InspectorItem it;
                it.label = itemDict.get("label", Value("BTN")).asString();
                it.id = title + "_" + it.label;
                std::string act = itemDict.get("action", Value("script")).asString();
                it.floatParam = itemDict.get("val", Value(0.0)).asFloat(0.0f);
                it.intParam = static_cast<int>(it.floatParam);
                it.width = itemDict.get("width", Value(108.0)).asFloat(108.0f);
                it.height = 24.0f;

                if (act == "transpose") it.action = SelectionSidebarAction::Transpose;
                else if (act == "nudge") it.action = SelectionSidebarAction::Nudge;
                else if (act == "duration") it.action = SelectionSidebarAction::SetDuration;
                else if (act == "velocity") it.action = SelectionSidebarAction::SetVelocity;
                else if (act == "humanize") it.action = SelectionSidebarAction::Humanize;
                else if (act == "quantize") it.action = SelectionSidebarAction::Quantize;
                else if (act == "retrograde") it.action = SelectionSidebarAction::Retrograde;
                else if (act == "invert_pitch") it.action = SelectionSidebarAction::InvertPitch;
                else {
                    it.action = SelectionSidebarAction::Script;
                    it.scriptCode = itemDict.get("script", Value(act)).asString();
                }

                currentRow.items.push_back(it);
                if (currentRow.items.size() >= 2) {
                    sec.rows.push_back(currentRow);
                    currentRow = InspectorRow{};
                    currentRow.rowHeight = 24.0f;
                    currentRow.marginBottom = 4.0f;
                }
            }
            if (!currentRow.items.empty()) {
                sec.rows.push_back(currentRow);
            }
        }

        addSection(sec);
        return Value(true);
    });
}

} // namespace eatsbits::ui
