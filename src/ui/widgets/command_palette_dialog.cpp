#include "eatsbits/ui/widgets/command_palette_dialog.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include <algorithm>
#include <cctype>

namespace eatsbits::ui {

CommandPaletteDialog::CommandPaletteDialog() {
    rebuildFilteredList();
}

void CommandPaletteDialog::open() noexcept {
    isOpen_ = true;
    searchQuery_.clear();
    selectedCategory_ = CommandCategory::All;
    selectedIndex_ = 0;
    scrollOffset_ = 0.0f;
    rebuildFilteredList();
}

void CommandPaletteDialog::close() noexcept {
    isOpen_ = false;
    if (onClose) onClose();
}

void CommandPaletteDialog::toggle() noexcept {
    if (isOpen_) close();
    else open();
}

void CommandPaletteDialog::registerCommand(const QuickCommand& cmd) {
    allCommands_.push_back(cmd);
    rebuildFilteredList();
}

void CommandPaletteDialog::clearCommands() noexcept {
    allCommands_.clear();
    filteredCommands_.clear();
    selectedIndex_ = 0;
}

void CommandPaletteDialog::setQuery(const std::string& q) noexcept {
    searchQuery_ = q;
    selectedIndex_ = 0;
    rebuildFilteredList();
}

void CommandPaletteDialog::setSelectedCategory(CommandCategory cat) noexcept {
    selectedCategory_ = cat;
    selectedIndex_ = 0;
    rebuildFilteredList();
}

void CommandPaletteDialog::executeSelected() noexcept {
    if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(filteredCommands_.size())) {
        const auto* cmd = filteredCommands_[selectedIndex_];
        close();
        if (cmd && cmd->onExecute) {
            cmd->onExecute();
        }
    }
}

Color CommandPaletteDialog::getCategoryColor(CommandCategory cat) noexcept {
    switch (cat) {
        case CommandCategory::View:   return Color(0.0f, 1.0f, 0.40f, 1.0f);  // Neon Green
        case CommandCategory::Action: return Color(0.13f, 0.96f, 0.91f, 1.0f); // Cyan
        case CommandCategory::Preset: return Color(1.0f, 0.55f, 0.0f, 1.0f);  // Orange
        case CommandCategory::Theme:  return Color(0.74f, 0.0f, 1.0f, 1.0f);  // Purple
        case CommandCategory::Macro:  return Color(1.0f, 0.0f, 0.33f, 1.0f);  // Pink/Coral
        case CommandCategory::All:
        default:                      return Color(0.85f, 0.85f, 0.85f, 1.0f);
    }
}

std::string CommandPaletteDialog::getCategoryName(CommandCategory cat) noexcept {
    switch (cat) {
        case CommandCategory::All:    return "ALL";
        case CommandCategory::View:   return "VIEW";
        case CommandCategory::Action: return "ACTION";
        case CommandCategory::Preset: return "PRESET";
        case CommandCategory::Theme:  return "THEME";
        case CommandCategory::Macro:  return "MACRO";
        default:                      return "OTHER";
    }
}

void CommandPaletteDialog::rebuildFilteredList() noexcept {
    filteredCommands_.clear();

    auto toLowerStr = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    };
    std::string lowerQ = toLowerStr(searchQuery_);

    for (const auto& cmd : allCommands_) {
        // 1. Category check
        if (selectedCategory_ != CommandCategory::All && cmd.category != selectedCategory_) {
            continue;
        }

        // 2. Query substring matching
        if (!lowerQ.empty()) {
            std::string tLower = toLowerStr(cmd.title);
            std::string subLower = toLowerStr(cmd.subtitle);
            std::string idLower = toLowerStr(cmd.id);
            if (tLower.find(lowerQ) == std::string::npos &&
                subLower.find(lowerQ) == std::string::npos &&
                idLower.find(lowerQ) == std::string::npos) {
                continue;
            }
        }

        filteredCommands_.push_back(&cmd);
    }

    if (filteredCommands_.empty()) {
        selectedIndex_ = 0;
    } else {
        selectedIndex_ = std::clamp(selectedIndex_, 0, static_cast<int>(filteredCommands_.size()) - 1);
    }
}

void CommandPaletteDialog::layout(float screenW, float screenH) noexcept {
    screenWidth_ = screenW;
    screenHeight_ = screenH;

    const float dlgW = std::min(580.0f, screenW - 40.0f);
    const float dlgH = std::min(440.0f, screenH - 60.0f);
    const float dlgX = (screenW - dlgW) * 0.5f;
    const float dlgY = (screenH - dlgH) * 0.35f; // Position slightly above vertical center

    dialogBounds_ = Rect2D(dlgX, dlgY, dlgW, dlgH);

    // Search Box at top
    searchBoxBounds_ = Rect2D(dlgX + 16.0f, dlgY + 16.0f, dlgW - 32.0f, 36.0f);

    // Category filter pills row
    const float catBarY = searchBoxBounds_.y + searchBoxBounds_.h + 10.0f;
    categoryBarBounds_ = Rect2D(dlgX + 16.0f, catBarY, dlgW - 32.0f, 22.0f);

    categoryPillBounds_.clear();
    const float pillGap = 6.0f;
    const float pillW = (categoryBarBounds_.w - 5.0f * pillGap) / 6.0f;
    for (size_t i = 0; i < 6; ++i) {
        float px = categoryBarBounds_.x + static_cast<float>(i) * (pillW + pillGap);
        categoryPillBounds_.push_back(Rect2D(px, catBarY, pillW, 22.0f));
    }

    // Results list area
    const float listY = catBarY + 28.0f;
    const float listH = dlgY + dlgH - listY - 26.0f; // Leave 26px for footer
    resultsListBounds_ = Rect2D(dlgX + 16.0f, listY, dlgW - 32.0f, listH);
}

void CommandPaletteDialog::update(float dt) noexcept {
    (void)dt;
}

void CommandPaletteDialog::render(BatchRenderer2D& r, const ThemeTokens& theme) noexcept {
    if (!isOpen_) return;

    // 1. Frosted Backdrop Blur & Dark Dimming
    r.applyBackdropBlur(4.0f, 0.60f);
    r.drawRect(0.0f, 0.0f, screenWidth_, screenHeight_, 0.0f, 0.0f, 0.0f, 0.65f);

    // 2. Dialog Chassis Background & Deep Shadow
    drawRoundedRect(r, dialogBounds_.x, dialogBounds_.y, dialogBounds_.w, dialogBounds_.h, 8.0f,
                    0.10f, 0.11f, 0.13f, 0.98f);
    drawRoundedRectOutline(r, dialogBounds_.x, dialogBounds_.y, dialogBounds_.w, dialogBounds_.h, 8.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.8f, 1.2f);

    // 3. Search Box with Search Icon and Query Text
    drawRoundedRect(r, searchBoxBounds_.x, searchBoxBounds_.y, searchBoxBounds_.w, searchBoxBounds_.h, 6.0f,
                    0.06f, 0.07f, 0.08f, 0.95f);
    drawRoundedRectOutline(r, searchBoxBounds_.x, searchBoxBounds_.y, searchBoxBounds_.w, searchBoxBounds_.h, 6.0f,
                           theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.75f, 1.2f);

    // Search Prompt Icon (">")
    drawMonoText(r, ">", searchBoxBounds_.x + 12.0f, searchBoxBounds_.y + 10.0f, 14.0f, theme.primaryAccent);

    // Query text or Placeholder
    if (searchQuery_.empty()) {
        drawText(r, "Type a command or filter (ESC to dismiss)...",
                 searchBoxBounds_.x + 30.0f, searchBoxBounds_.y + 11.0f, 11.0f,
                 theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.55f);
    } else {
        drawText(r, searchQuery_, searchBoxBounds_.x + 30.0f, searchBoxBounds_.y + 11.0f, 12.0f,
                 theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);
    }

    // 4. Category Filter Pills
    static const CommandCategory categories[] = {
        CommandCategory::All, CommandCategory::View, CommandCategory::Action,
        CommandCategory::Preset, CommandCategory::Theme, CommandCategory::Macro
    };

    for (size_t i = 0; i < 6 && i < categoryPillBounds_.size(); ++i) {
        const auto& pb = categoryPillBounds_[i];
        bool isSelected = (selectedCategory_ == categories[i]);
        Color catCol = getCategoryColor(categories[i]);

        drawButton(r, pb, getCategoryName(categories[i]),
                   isSelected ? catCol.withAlpha(0.25f) : Color(0.14f, 0.15f, 0.18f, 0.90f),
                   isSelected ? catCol : theme.borderSubtle,
                   isSelected ? catCol : theme.textMuted,
                   8.5f, 3.0f, 1.0f);
    }

    // 5. Results List
    const float itemH = 46.0f;
    const float itemGap = 4.0f;
    const size_t maxVisible = static_cast<size_t>(resultsListBounds_.h / (itemH + itemGap));

    if (filteredCommands_.empty()) {
        drawCenteredText(r, "No matching commands found.", resultsListBounds_, 11.0f, theme.textMuted);
    } else {
        // Adjust scrollOffset to keep selectedIndex visible
        float targetScroll = static_cast<float>(selectedIndex_) * (itemH + itemGap);
        if (targetScroll < scrollOffset_) {
            scrollOffset_ = targetScroll;
        } else if (targetScroll + itemH > scrollOffset_ + resultsListBounds_.h) {
            scrollOffset_ = targetScroll + itemH - resultsListBounds_.h;
        }

        size_t startIndex = static_cast<size_t>(std::max(0.0f, scrollOffset_ / (itemH + itemGap)));
        size_t endIndex = std::min(filteredCommands_.size(), startIndex + maxVisible + 1);

        for (size_t i = startIndex; i < endIndex; ++i) {
            const auto* cmd = filteredCommands_[i];
            float itemY = resultsListBounds_.y + static_cast<float>(i) * (itemH + itemGap) - scrollOffset_;
            if (itemY + itemH < resultsListBounds_.y || itemY > resultsListBounds_.y + resultsListBounds_.h) continue;

            Rect2D itemBounds(resultsListBounds_.x, itemY, resultsListBounds_.w, itemH);
            bool isSelected = (static_cast<int>(i) == selectedIndex_);

            // Item Background
            Color itemBg = isSelected
                               ? theme.primaryAccent.withAlpha(0.20f)
                               : Color(0.12f, 0.13f, 0.16f, 0.85f);
            drawRoundedRect(r, itemBounds.x, itemBounds.y, itemBounds.w, itemBounds.h, 4.0f, itemBg);

            if (isSelected) {
                drawRoundedRectOutline(r, itemBounds.x, itemBounds.y, itemBounds.w, itemBounds.h, 4.0f,
                                       theme.primaryAccent, 1.2f);
                // Left accent notch
                drawRect(r, itemBounds.x, itemBounds.y + 4.0f, 3.0f, itemBounds.h - 8.0f, theme.primaryAccent);
            }

            // Category Badge
            Color catCol = getCategoryColor(cmd->category);
            drawRoundedRect(r, itemBounds.x + 10.0f, itemBounds.y + 14.0f, 44.0f, 16.0f, 3.0f,
                            catCol.withAlpha(0.18f));
            drawCenteredText(r, getCategoryName(cmd->category), itemBounds.x + 10.0f, itemBounds.y + 14.0f,
                             44.0f, 16.0f, 7.5f, catCol);

            // Title
            Color titleCol = isSelected ? Color(1.0f, 1.0f, 1.0f, 1.0f) : theme.textPrimary;
            drawText(r, cmd->title, itemBounds.x + 62.0f, itemBounds.y + 8.0f, 10.5f, titleCol);

            // Subtitle
            drawText(r, cmd->subtitle, itemBounds.x + 62.0f, itemBounds.y + 24.0f, 8.5f, theme.textMuted);

            // Shortcut Badge (if any)
            if (!cmd->shortcutHint.empty()) {
                const float scW = 68.0f;
                const float scH = 18.0f;
                float scX = itemBounds.x + itemBounds.w - scW - 10.0f;
                float scY = itemBounds.y + (itemBounds.h - scH) * 0.5f;

                drawRoundedRect(r, scX, scY, scW, scH, 3.0f, Color(0.08f, 0.09f, 0.11f, 0.90f));
                drawRoundedRectOutline(r, scX, scY, scW, scH, 3.0f, theme.borderSubtle, 1.0f);
                drawCenteredText(r, cmd->shortcutHint, scX, scY, scW, scH, 8.0f, theme.textSecondary);
            }
        }
    }

    // 6. Bottom Footer Hint
    drawCenteredText(r, "^ / v to navigate  *  ENTER to execute  *  ESC to dismiss",
                     dialogBounds_.x, dialogBounds_.y + dialogBounds_.h - 22.0f,
                     dialogBounds_.w, 16.0f, 8.5f, theme.textMuted);
}

bool CommandPaletteDialog::handlePointer(const PointerEvent& ev) noexcept {
    if (!isOpen_) return false;

    // Click outside modal -> dismiss
    if (!dialogBounds_.contains(ev.x, ev.y)) {
        if (ev.action == PointerAction::Down) {
            close();
            return true;
        }
        return true;
    }

    // 1. Category Pills Click
    if (ev.action == PointerAction::Down) {
        static const CommandCategory categories[] = {
            CommandCategory::All, CommandCategory::View, CommandCategory::Action,
            CommandCategory::Preset, CommandCategory::Theme, CommandCategory::Macro
        };
        for (size_t i = 0; i < 6 && i < categoryPillBounds_.size(); ++i) {
            if (categoryPillBounds_[i].contains(ev.x, ev.y)) {
                setSelectedCategory(categories[i]);
                return true;
            }
        }
    }

    // 2. Results List Click / Hover
    if (resultsListBounds_.contains(ev.x, ev.y)) {
        const float itemH = 46.0f;
        const float itemGap = 4.0f;
        float relY = ev.y - resultsListBounds_.y + scrollOffset_;
        int hitIndex = static_cast<int>(relY / (itemH + itemGap));

        if (hitIndex >= 0 && hitIndex < static_cast<int>(filteredCommands_.size())) {
            if (ev.action == PointerAction::Move) {
                selectedIndex_ = hitIndex;
                return true;
            }
            if (ev.action == PointerAction::Down) {
                selectedIndex_ = hitIndex;
                executeSelected();
                return true;
            }
        }

        // Wheel Scroll
        if (ev.action == PointerAction::Scroll) {
            scrollOffset_ = std::max(0.0f, scrollOffset_ - ev.scrollY * 30.0f);
            return true;
        }
    }

    return true; // Absorb all interactions inside modal dialog
}

bool CommandPaletteDialog::handleKey(int key, int scancode, int action, int mods) noexcept {
    (void)scancode;
    (void)mods;
    if (!isOpen_) return false;
    if (action != 1 && action != 2) return true; // Only press and repeat

    // ESC (256 in GLFW)
    if (key == 256) {
        close();
        return true;
    }

    // Down Arrow (264 in GLFW)
    if (key == 264) {
        if (!filteredCommands_.empty()) {
            selectedIndex_ = (selectedIndex_ + 1) % filteredCommands_.size();
        }
        return true;
    }

    // Up Arrow (265 in GLFW)
    if (key == 265) {
        if (!filteredCommands_.empty()) {
            selectedIndex_ = (selectedIndex_ - 1 + static_cast<int>(filteredCommands_.size())) % static_cast<int>(filteredCommands_.size());
        }
        return true;
    }

    // Enter (257 in GLFW)
    if (key == 257) {
        executeSelected();
        return true;
    }

    // Backspace (259 in GLFW)
    if (key == 259) {
        if (!searchQuery_.empty()) {
            searchQuery_.pop_back();
            selectedIndex_ = 0;
            rebuildFilteredList();
        }
        return true;
    }

    return true; // Absorb keys while palette is open
}

bool CommandPaletteDialog::handleChar(unsigned int codepoint) noexcept {
    if (!isOpen_) return false;

    if (codepoint >= 32 && codepoint < 127) {
        searchQuery_ += static_cast<char>(codepoint);
        selectedIndex_ = 0;
        rebuildFilteredList();
        return true;
    }
    return true;
}

} // namespace eatsbits::ui
