#include "eatsbits/ui/widgets/project_browser_drawer.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include "eatsbits/ui/icon_registry.hpp"
#include "eatsbits/project/preset_manager.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <chrono>

namespace eatsbits::ui {

ProjectBrowserDrawer::ProjectBrowserDrawer() {
    initData();
    scanSavedProjects();
}

void ProjectBrowserDrawer::initData() {
    // Default fallback tracks (will be overwritten if setTracks is called)
    tracks_ = {
        {0, "Eats-303 Acid", "SYNTH", 4, false, false, Color(0.0f, 0.95f, 1.0f, 1.0f)},
        {1, "Eats-909 Drums", "DRUMS", 2, false, false, Color(1.0f, 0.65f, 0.0f, 1.0f)},
        {2, "Eats-808 Sub", "DRUMS", 1, false, false, Color(0.95f, 0.20f, 0.50f, 1.0f)},
        {3, "Retro SNES Synth", "SYNTH", 3, false, false, Color(0.40f, 0.85f, 0.30f, 1.0f)},
        {4, "Acid Echo FX", "AUDIO", 0, false, false, Color(0.70f, 0.40f, 1.0f, 1.0f)}
    };

    // Tab 2: Script & Engine Library
    scripts_ = {
        {"eats_303", "Acid Odyssey 303", "SYNTH", "Authentic Roland TB-303 Diode Ladder", "Roland / Acid"},
        {"poly_saw", "PolyBLEP Super-Saw", "SYNTH", "16-Voice Anti-Aliased PolyBLEP Oscillators", "Poly / Analog"},
        {"fm_dx7", "DX7 6-Op FM Synth", "SYNTH", "Yamaha DX7 32-Algorithm Dual Bus FM", "Yamaha / Digital"},
        {"chip_snes", "SNES Chrono Synth", "SYNTH", "SPC700 16-Bit S-DSP with 8-Tap FIR Echo", "Nintendo / Retro"},
        {"chip_ym2612", "Genesis Thunder FM", "SYNTH", "OPN2 4-Operator Logarithmic Feedback", "Sega / Chiptune"},
        {"piano_grand", "Concert Grand Piano", "SYNTH", "Bank-Bensa Commuted Waveguide Resonator", "Physical / Keys"},
        {"upright_bass", "Upright Double Bass", "SYNTH", "Ebony Fingerboard Collision Growl", "Physical / Bass"},
        {"stereo_delay", "Ping-Pong Stereo Delay", "AUDIO_FX", "Cross-Feedback Analog Tape Delay", "FX / Delay"},
        {"analog_chorus", "Studio BBD Chorus", "AUDIO_FX", "Dual Bucket-Brigade Dimension Chorus", "FX / Modulation"},
        {"convolver_verb", "True Zero-Latency Reverb", "AUDIO_FX", "Zero-Latency Real-Time Stereo Convolution", "FX / Reverb"},
        {"bitcrusher", "Vintage Sampler Crusher", "AUDIO_FX", "12-Bit Vintage S-DSP Sample & Hold", "FX / Distortion"},
        {"arp_generator", "Euclidean Arpeggiator", "MIDI_FX", "Algorithmic Euclidean & Classic Arp", "MIDI / Pattern"},
        {"scale_snap", "Harmonic Scale Snap", "MIDI_FX", "Quantizes raw notes to selected musical scale", "MIDI / Theory"},
        {"humanize", "Global Humanizer", "MIDI_FX", "Micro-timing and velocity drift generator", "MIDI / Groove"},
        {"acid_seq_01", "Classic Acid 303 Riff", "MIDI_SEQ", "16-Step energetic 16th-note acid bassline", "Sequence / Acid"},
        {"four_floor_seq", "Techno 4-on-the-Floor", "MIDI_SEQ", "Standard 909 kick, snappy snare and hats", "Sequence / Techno"},
        {"macro_acid", "Acid 303 Generator", "MACRO", "Generates randomized Roland acid lines", "Macro / AI"},
        {"macro_909", "909 Techno Synthesizer", "MACRO", "Generates 4-on-the-floor industrial beats", "Macro / Rhythm"},
        {"macro_humanize", "Batch Humanize", "MACRO", "Applies subtle velocity and timing variations", "Macro / Groove"},
        {"macro_song", "Procedural Song Architect", "MACRO", "Generates complete multi-track song arrangement", "Macro / Generative"}
    };

    // Tab 3: Sound Patches, Spaces & Cabs (Populated from PresetManager catalog)
    project::PresetManager::instance().initialize();
    const auto& catalog = project::PresetManager::instance().getAllPresets();
    if (!catalog.empty()) {
        patches_.clear();
        for (const auto& item : catalog) {
            BrowserSoundPatchItem p;
            p.id = item.id;
            p.name = item.name;
            p.category = item.category;
            p.engineTag = item.engineTag;
            p.description = item.description;
            p.meta = (item.author.empty() || item.author == "Eatsbeats / Eatsbits") ? item.engineTag : item.author;
            patches_.push_back(p);
        }
    } else {
        patches_ = {
            {"acid_303", "Acid Odyssey 303", "BASS", "TB-303", "Authentic Roland TB-303 Diode Ladder", "Roland Diode Ladder"},
            {"sub_808", "Sub Resonance 808", "BASS", "TR-808", "Bridged-T Sine Sub Bass with analog punch", "Bridged-T Sine"},
            {"upright_growl", "Upright Double Bass", "BASS", "PHYSICAL", "Ebony Fingerboard Collision Growl", "Physical String"},
            {"sid_lead", "C64 SID Space Arp", "LEAD", "SID", "MOS 6581 3-Voice 23-bit Noise Arp", "MOS 6581 Chiptune"},
            {"genesis_lead", "Genesis Thunder FM", "LEAD", "YM2612", "OPN2 4-Op Logarithmic Feedback Lead", "OPN2 4-Op FM"},
            {"grand_piano", "Concert Grand Piano", "PLUCK", "PHYSICAL", "Bank-Bensa Commuted Waveguide Resonator", "Acoustic Piano"},
            {"spanish_nylon", "Spanish Classical Guitar", "PLUCK", "PHYSICAL", "Torres Body Resonator with nylon strings", "Acoustic Guitar"},
            {"steel_acoustic", "Steel String Acoustic", "PLUCK", "PHYSICAL", "Plectrum Multi-Tap Comb Resonator", "Acoustic Guitar"},
            {"dx7_rhodes", "DX7 Electric Piano", "PLUCK", "DX7", "Algorithm 5 Dual Bus FM Electric Piano", "FM 6-Operator"},
            {"snes_pad", "SNES Chrono Echo Pad", "PAD", "SNES", "SPC700 16-Bit S-DSP 8-Tap FIR Echo", "16-Bit S-DSP"},
            {"tr909_tech", "TR-909 Techno Kit", "DRUMS", "TR-909", "Punchy 909 Kick, Snare, Clap, and Hats", "Analog 909 Kit"},
            {"tr808_kit", "TR-808 Classic Kit", "DRUMS", "TR-808", "Deep analog 808 kick, congas, cowbell", "Analog 808 Kit"}
        };
    }

    // Tab 4: Expansion Packs
    packs_ = {
        {"tool_audio2midi", "Neural Audio-to-MIDI Converter", "Transcribes WAV/MP3 recordings directly into MIDI tracks", 4.5f, true, "AI_TOOL"},
        {"sf_gm_essential", "General MIDI Essential SoundFont", "Full General MIDI 128-instrument soundbank", 18.4f, true, "SOUNDFONT"},
        {"sf_drums_vintage", "Vintage 808/909 Acoustic Drums", "Multi-sample velocity layered drum machines", 12.2f, true, "SOUNDFONT"},
        {"sf_upright_bass", "Upright Double Bass SoundFont", "Multi-articulation acoustic bass library", 28.6f, true, "SOUNDFONT"},
        {"sf_steinway", "Steinway Model D Concert Grand", "High-fidelity velocity switched concert piano", 45.0f, false, "SOUNDFONT"}
    };

    // Tab 6: Fallback History
    history_ = {
        {0, "Project Initialized", "SYSTEM", "00:00", true, false},
        {1, "Added Track Eats-303 Acid", "ARRANGE", "00:02", false, false},
        {2, "Inserted 16-Step Bassline Pattern", "NOTE", "00:05", false, false},
        {3, "Tweaked Cutoff Frequency to 1850 Hz", "TUNE", "00:12", false, true}
    };
    canUndo_ = true;
    canRedo_ = false;
}

void ProjectBrowserDrawer::scanSavedProjects() {
    savedProjects_.clear();
    std::vector<std::string> searchDirs = {"./Projects", "."};

    std::error_code ec;
    if (!std::filesystem::exists("./Projects", ec)) {
        std::filesystem::create_directories("./Projects", ec);
    }

    for (const auto& dirPath : searchDirs) {
        if (!std::filesystem::exists(dirPath, ec)) continue;

        for (const auto& entry : std::filesystem::directory_iterator(dirPath, ec)) {
            if (entry.is_regular_file(ec) && entry.path().extension() == ".eats") {
                BrowserSavedProjectItem item;
                item.fileName = entry.path().filename().string();
                item.filePath = entry.path().string();
                item.name = entry.path().stem().string();
                item.sizeBytes = static_cast<uint64_t>(entry.file_size(ec));

                auto ftime = entry.last_write_time(ec);
                auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
                std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);
                std::tm tmBuf{};
#if defined(_WIN32)
                localtime_s(&tmBuf, &cftime);
#else
                localtime_r(&cftime, &tmBuf);
#endif
                std::ostringstream ss;
                ss << std::put_time(&tmBuf, "%Y-%m-%d %H:%M");
                item.lastModified = ss.str();

                savedProjects_.push_back(item);
            }
        }
    }

    if (savedProjects_.empty()) {
        BrowserSavedProjectItem demoItem;
        demoItem.name = "Acid Odyssey 303";
        demoItem.fileName = "acid_odyssey_303.eats";
        demoItem.filePath = "./Projects/acid_odyssey_303.eats";
        demoItem.sizeBytes = 43589;
        demoItem.lastModified = "2026-09-27 05:30";
        savedProjects_.push_back(demoItem);
    }
}

std::string ProjectBrowserDrawer::formatBytes(uint64_t bytes) const {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << (static_cast<double>(bytes) / 1024.0) << " KB";
        return ss.str();
    }
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << (static_cast<double>(bytes) / (1024.0 * 1024.0)) << " MB";
    return ss.str();
}

void ProjectBrowserDrawer::setTracks(const std::vector<BrowserTrackAssetItem>& tracks) {
    tracks_ = tracks;
}

void ProjectBrowserDrawer::setHistory(const std::vector<BrowserHistoryMilestoneItem>& history, bool canUndo, bool canRedo) {
    history_ = history;
    canUndo_ = canUndo;
    canRedo_ = canRedo;
}

void ProjectBrowserDrawer::open() noexcept {
    isOpen_ = true;
    scanSavedProjects();
}

void ProjectBrowserDrawer::close() noexcept {
    isOpen_ = false;
    if (onClose) onClose();
}

void ProjectBrowserDrawer::toggle() noexcept {
    if (isOpen_) close();
    else open();
}

void ProjectBrowserDrawer::setTab(BrowserDrawerTab tab) noexcept {
    activeTab_ = tab;
    selectedIndex_ = 0;
    scrollOffset_ = 0.0f;
    selectedCategory_ = "ALL";
    selectedScriptCategory_ = "ALL";
    isDraggingScroll_ = false;
    if (tab == BrowserDrawerTab::Projects) {
        scanSavedProjects();
    }
}

void ProjectBrowserDrawer::setScriptCategoryFilter(const std::string& cat) noexcept {
    selectedScriptCategory_ = cat;
    selectedIndex_ = 0;
    scrollOffset_ = 0.0f;
    isDraggingScroll_ = false;
}

float ProjectBrowserDrawer::computeMaxScroll() const {
    float contentY = (activeTab_ == BrowserDrawerTab::Scripts) ?
                     (scriptCategoryBounds_[0].y + scriptCategoryBounds_[0].h + 8.0f) :
                     (searchBoxBounds_.y + searchBoxBounds_.h + 8.0f);
    float availH = drawerBounds_.y + drawerBounds_.h - contentY - 8.0f;
    return computeMaxScroll(contentY, availH);
}

float ProjectBrowserDrawer::computeMaxScroll([[maybe_unused]] float contentY, float availH) const {
    float totalH = 0.0f;
    switch (activeTab_) {
        case BrowserDrawerTab::Assets: {
            float cardH = 40.0f;
            totalH = static_cast<float>(tracks_.size()) * (cardH + 6.0f);
            break;
        }
        case BrowserDrawerTab::Scripts: {
            float cardH = 46.0f;
            totalH = static_cast<float>(getFilteredScriptsCount()) * (cardH + 6.0f);
            break;
        }
        case BrowserDrawerTab::Presets: {
            float cardH = 46.0f;
            totalH = static_cast<float>(getFilteredPatchesCount()) * (cardH + 6.0f);
            break;
        }
        case BrowserDrawerTab::Packs: {
            float toolCardH = 50.0f;
            float packH = 46.0f;
            totalH = toolCardH + 34.0f + static_cast<float>(packs_.empty() ? 0 : packs_.size() - 1) * (packH + 6.0f);
            break;
        }
        case BrowserDrawerTab::Projects: {
            float actH = 32.0f;
            float cardH = 50.0f;
            totalH = actH + static_cast<float>(getFilteredProjectsCount()) * (cardH + 6.0f);
            break;
        }
        case BrowserDrawerTab::History: {
            float toolH = 32.0f;
            float cardH = 38.0f;
            totalH = toolH + static_cast<float>(history_.size()) * (cardH + 6.0f);
            break;
        }
    }
    return std::max(0.0f, totalH - availH);
}

size_t ProjectBrowserDrawer::getFilteredScriptsCount() const {
    size_t count = 0;
    std::string q = searchQuery_;
    std::transform(q.begin(), q.end(), q.begin(), ::tolower);
    for (const auto& item : scripts_) {
        if (selectedScriptCategory_ != "ALL") {
            if (selectedScriptCategory_ == "SYNTH" && item.category != "SYNTH") continue;
            if (selectedScriptCategory_ == "AUDIO_FX" && item.category != "AUDIO_FX") continue;
            if (selectedScriptCategory_ == "MIDI_FX" && item.category != "MIDI_FX") continue;
            if (selectedScriptCategory_ == "MIDI_SEQ" && item.category != "MIDI_SEQ") continue;
            if (selectedScriptCategory_ == "MACRO" && item.category != "MACRO") continue;
        }
        if (!q.empty()) {
            std::string nameLower = item.name;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
            std::string catLower = item.category;
            std::transform(catLower.begin(), catLower.end(), catLower.begin(), ::tolower);
            std::string descLower = item.description;
            std::transform(descLower.begin(), descLower.end(), descLower.begin(), ::tolower);
            if (nameLower.find(q) == std::string::npos && catLower.find(q) == std::string::npos && descLower.find(q) == std::string::npos) {
                continue;
            }
        }
        count++;
    }
    return count;
}

size_t ProjectBrowserDrawer::getFilteredPatchesCount() const {
    size_t count = 0;
    std::string q = searchQuery_;
    std::transform(q.begin(), q.end(), q.begin(), ::tolower);
    for (const auto& item : patches_) {
        if (selectedCategory_ != "ALL" && item.category != selectedCategory_) continue;
        if (!q.empty()) {
            std::string nameLower = item.name;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
            std::string catLower = item.category;
            std::transform(catLower.begin(), catLower.end(), catLower.begin(), ::tolower);
            std::string descLower = item.description;
            std::transform(descLower.begin(), descLower.end(), descLower.begin(), ::tolower);
            if (nameLower.find(q) == std::string::npos && catLower.find(q) == std::string::npos && descLower.find(q) == std::string::npos) {
                continue;
            }
        }
        count++;
    }
    return count;
}

size_t ProjectBrowserDrawer::getFilteredProjectsCount() const {
    size_t count = 0;
    std::string q = searchQuery_;
    std::transform(q.begin(), q.end(), q.begin(), ::tolower);
    for (const auto& proj : savedProjects_) {
        if (!q.empty()) {
            std::string nameLower = proj.name;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
            if (nameLower.find(q) == std::string::npos && proj.fileName.find(q) == std::string::npos) {
                continue;
            }
        }
        count++;
    }
    return count;
}

void ProjectBrowserDrawer::layout(float screenWidth, float screenHeight, float topHeaderHeight, float bottomNavHeight) {
    isMobile_ = (screenWidth < 768.0f);
    float drawerH = screenHeight - topHeaderHeight - bottomNavHeight;
    float currentX = screenWidth - (kDrawerWidth * animProgress_);

    drawerBounds_ = Rect2D(currentX, topHeaderHeight, kDrawerWidth, drawerH);

    float headerY = topHeaderHeight + 8.0f;
    closeBtnBounds_ = Rect2D(currentX + kDrawerWidth - 32.0f, headerY, 24.0f, 24.0f);

    // 6 Tabs layout
    float tabY = headerY + 28.0f;
    float totalTabW = kDrawerWidth - 24.0f;
    float tabW = totalTabW / 6.0f;
    float tabH = isMobile_ ? 26.0f : 28.0f;

    for (size_t i = 0; i < 6; ++i) {
        tabBounds_[i] = Rect2D(currentX + 12.0f + (static_cast<float>(i) * tabW), tabY, tabW - 2.0f, tabH);
    }

    // Search bar below tab strip
    float subY = tabY + tabH + 6.0f;
    searchBoxBounds_ = Rect2D(currentX + 12.0f, subY, kDrawerWidth - 24.0f, 24.0f);

    // Script category filter chips (below search bar)
    float chipY = subY + 28.0f;
    float totalChipW = kDrawerWidth - 24.0f;
    float chipW = (totalChipW - (5.0f * 4.0f)) / 6.0f;
    for (size_t i = 0; i < 6; ++i) {
        scriptCategoryBounds_[i] = Rect2D(currentX + 12.0f + (static_cast<float>(i) * (chipW + 4.0f)), chipY, chipW, 22.0f);
    }
}

void ProjectBrowserDrawer::update(float dt) {
    float target = isOpen_ ? 1.0f : 0.0f;
    constexpr float kDrawerDamping = 18.0f; // ~180ms smooth ease-out
    float factor = 1.0f - std::exp(-kDrawerDamping * dt);
    animProgress_ += (target - animProgress_) * factor;

    if (std::abs(animProgress_ - target) < 0.001f) {
        animProgress_ = target;
    }

    animOffset_ = kDrawerWidth * (1.0f - animProgress_);
}

void ProjectBrowserDrawer::render(BatchRenderer2D& r, const ThemeTokens& theme) {
    if (animProgress_ <= 0.001f) return;

    // Drawer chassis background
    drawRect(r, drawerBounds_.x, drawerBounds_.y, drawerBounds_.w, drawerBounds_.h,
             theme.panelBackground.r * 0.88f, theme.panelBackground.g * 0.88f, theme.panelBackground.b * 0.88f, 0.98f);

    // Left border indicator line
    drawLine(r, drawerBounds_.x, drawerBounds_.y, drawerBounds_.x, drawerBounds_.y + drawerBounds_.h,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.8f * animProgress_, 2.0f);

    // Header Title
    drawText(r, "PROJECT BROWSER", drawerBounds_.x + 14.0f, drawerBounds_.y + 12.0f, 12.5f,
             theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);

    // Close Button (metallic screw icon)
    float cx = closeBtnBounds_.x + closeBtnBounds_.w * 0.5f;
    float cy = closeBtnBounds_.y + closeBtnBounds_.h * 0.5f;
    bool closeHov = closeBtnBounds_.contains(mouseX_, mouseY_) || std::hypot(mouseX_ - cx, mouseY_ - cy) <= 12.0f;
    drawScrewCloseButton(r, cx, cy, 9.0f, closeHov, theme.primaryAccent);

    // 6 Tab Strip with Flutter Icons (Mobile: icon only; Desktop: icon + label)
    const char* tabIcons[6] = {"ui_tab_assets", "ui_tab_script", "ui_tab_preset", "ui_tab_packs", "ui_tab_projects", "ui_tab_history"};
    const char* tabNames[6] = {"ASSETS", "SCRIPT", "PRESET", "PACKS", "PROJ", "HIST"};
    for (size_t i = 0; i < 6; ++i) {
        bool active = (static_cast<size_t>(activeTab_) == i);
        const auto& tb = tabBounds_[i];
        bool hov = tb.contains(mouseX_, mouseY_);

        Color bg = active ? (theme.primaryAccent * 0.25f) : (hov ? theme.panelHeader * 1.25f : theme.panelHeader);
        Color border = active ? theme.primaryAccent : (hov ? theme.borderSubtle * 1.5f : Color(0.0f, 0.0f, 0.0f, 0.0f));
        Color fg = active ? theme.primaryAccent : (hov ? theme.textPrimary : theme.textMuted);

        drawRoundedRect(r, tb.x, tb.y, tb.w, tb.h, 4.0f, bg);
        if (active || hov) {
            drawRoundedRectOutline(r, tb.x, tb.y, tb.w, tb.h, 4.0f, border.r, border.g, border.b, active ? 0.95f : 0.5f, 1.0f);
        }

        if (isMobile_) {
            // Mobile: only icon centered for space efficiency
            float iconSize = 14.0f;
            float ix = tb.x + (tb.w - iconSize) * 0.5f;
            float iy = tb.y + (tb.h - iconSize) * 0.5f;
            IconRegistry::instance().renderIcon(r, tabIcons[i], Rect2D(ix, iy, iconSize, iconSize), fg);
        } else {
            // Desktop: Flutter icon + text
            float iconSize = 11.5f;
            float ix = tb.x + 4.5f;
            float iy = tb.y + (tb.h - iconSize) * 0.5f;
            IconRegistry::instance().renderIcon(r, tabIcons[i], Rect2D(ix, iy, iconSize, iconSize), fg);
            drawText(r, tabNames[i], tb.x + 18.0f, tb.y + 7.5f, 7.5f, fg.r, fg.g, fg.b, 1.0f);
        }
    }

    // Search bar or tab header
    if (activeTab_ == BrowserDrawerTab::Scripts || activeTab_ == BrowserDrawerTab::Presets || activeTab_ == BrowserDrawerTab::Projects) {
        drawRoundedRect(r, searchBoxBounds_.x, searchBoxBounds_.y, searchBoxBounds_.w, searchBoxBounds_.h, 4.0f,
                        theme.panelHeader.r * 0.7f, theme.panelHeader.g * 0.7f, theme.panelHeader.b * 0.7f, 0.9f);
        drawRoundedRectOutline(r, searchBoxBounds_.x, searchBoxBounds_.y, searchBoxBounds_.w, searchBoxBounds_.h, 4.0f,
                               theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.5f, 1.0f);
        std::string searchDisplay = searchQuery_.empty() ? ("Filter " + std::string(tabNames[static_cast<size_t>(activeTab_)]) + "...") : searchQuery_;
        drawText(r, searchDisplay, searchBoxBounds_.x + 8.0f, searchBoxBounds_.y + 6.0f, 9.5f,
                 searchQuery_.empty() ? theme.textMuted.r : theme.textPrimary.r,
                 searchQuery_.empty() ? theme.textMuted.g : theme.textPrimary.g,
                 searchQuery_.empty() ? theme.textMuted.b : theme.textPrimary.b, 0.85f);
    } else if (activeTab_ == BrowserDrawerTab::Assets) {
        drawText(r, "ACTIVE SESSION CHANNELS & FX INSERTS", searchBoxBounds_.x + 2.0f, searchBoxBounds_.y + 6.0f, 9.5f,
                 theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b, 0.9f);
    } else if (activeTab_ == BrowserDrawerTab::Packs) {
        drawText(r, "SOUNDBANKS & NEURAL TRANSCRIPTION", searchBoxBounds_.x + 2.0f, searchBoxBounds_.y + 6.0f, 9.5f,
                 theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b, 0.9f);
    } else if (activeTab_ == BrowserDrawerTab::History) {
        // Undo / Redo Toolbar
        float btnW = (searchBoxBounds_.w - 18.0f) / 4.0f;
        Rect2D undoRect(searchBoxBounds_.x, searchBoxBounds_.y, btnW, 24.0f);
        Rect2D redoRect(searchBoxBounds_.x + btnW + 6.0f, searchBoxBounds_.y, btnW, 24.0f);
        Rect2D checkptRect(searchBoxBounds_.x + (btnW + 6.0f) * 2.0f, searchBoxBounds_.y, btnW, 24.0f);
        Rect2D clearRect(searchBoxBounds_.x + (btnW + 6.0f) * 3.0f, searchBoxBounds_.y, btnW, 24.0f);

        drawButton(r, undoRect, "UNDO", canUndo_ ? theme.primaryAccent * 0.25f : theme.panelHeader,
                   canUndo_ ? theme.primaryAccent : theme.borderSubtle,
                   canUndo_ ? theme.primaryAccent : theme.textMuted, 8.5f, 3.0f, 1.0f);

        drawButton(r, redoRect, "REDO", canRedo_ ? theme.primaryAccent * 0.25f : theme.panelHeader,
                   canRedo_ ? theme.primaryAccent : theme.borderSubtle,
                   canRedo_ ? theme.primaryAccent : theme.textMuted, 8.5f, 3.0f, 1.0f);

        drawButton(r, checkptRect, "+ PIN", theme.secondaryAccent * 0.25f, theme.secondaryAccent, theme.secondaryAccent, 8.5f, 3.0f, 1.0f);
        drawButton(r, clearRect, "CLEAR", theme.panelHeader, theme.borderSubtle, theme.muteActive, 8.5f, 3.0f, 1.0f);
    }

    // Script Category Sub-sections Filter Strip (ALL, INST, FX, MIDI, SEQ, MACRO)
    if (activeTab_ == BrowserDrawerTab::Scripts) {
        const char* catIcons[6] = {"ui_cat_all", "ui_cat_instruments", "ui_cat_fx", "ui_cat_midifx", "ui_cat_seq", "ui_cat_macro"};
        const char* catNames[6] = {"ALL", "INST", "FX", "MIDI", "SEQ", "MACRO"};
        const char* catFilters[6] = {"ALL", "SYNTH", "AUDIO_FX", "MIDI_FX", "MIDI_SEQ", "MACRO"};
        const Color catColors[6] = {
            theme.primaryAccent,
            Color(1.0f, 0.55f, 0.0f, 1.0f),   // Instruments (Orange)
            Color(0.13f, 0.96f, 0.91f, 1.0f),  // Audio FX (Cyan)
            Color(0.0f, 1.0f, 0.4f, 1.0f),     // MIDI FX (Neon Green)
            Color(1.0f, 0.84f, 0.0f, 1.0f),    // Sequences (Gold)
            Color(0.74f, 0.0f, 1.0f, 1.0f)     // Macros (Purple)
        };

        for (size_t i = 0; i < 6; ++i) {
            const auto& cb = scriptCategoryBounds_[i];
            bool isSel = (selectedScriptCategory_ == catFilters[i]);
            bool hov = cb.contains(mouseX_, mouseY_);
            const auto& cCol = catColors[i];

            Color bg = isSel ? (cCol * 0.25f) : (hov ? (cCol * 0.12f) : Color(0.0f, 0.0f, 0.0f, 0.35f));
            Color border = isSel ? cCol : (hov ? (cCol * 0.6f) : theme.borderSubtle * 0.7f);
            Color fg = isSel ? cCol : (hov ? theme.textPrimary : (cCol * 0.75f));

            drawRoundedRect(r, cb.x, cb.y, cb.w, cb.h, 4.0f, bg);
            drawRoundedRectOutline(r, cb.x, cb.y, cb.w, cb.h, 4.0f, border.r, border.g, border.b, isSel ? 0.95f : 0.45f, 1.0f);

            if (isMobile_) {
                // Mobile: Only icon
                float icSize = 13.0f;
                float ix = cb.x + (cb.w - icSize) * 0.5f;
                float iy = cb.y + (cb.h - icSize) * 0.5f;
                IconRegistry::instance().renderIcon(r, catIcons[i], Rect2D(ix, iy, icSize, icSize), fg);
            } else {
                // Desktop: Icon + short label
                float icSize = 11.0f;
                float ix = cb.x + 3.5f;
                float iy = cb.y + (cb.h - icSize) * 0.5f;
                IconRegistry::instance().renderIcon(r, catIcons[i], Rect2D(ix, iy, icSize, icSize), fg);
                drawText(r, catNames[i], cb.x + 16.5f, cb.y + 6.0f, 7.5f, fg.r, fg.g, fg.b, 1.0f);
            }
        }
    }

    float contentY = (activeTab_ == BrowserDrawerTab::Scripts) ?
                     (scriptCategoryBounds_[0].y + scriptCategoryBounds_[0].h + 8.0f) :
                     (searchBoxBounds_.y + searchBoxBounds_.h + 8.0f);
    float availH = drawerBounds_.y + drawerBounds_.h - contentY - 8.0f;
    maxScroll_ = computeMaxScroll(contentY, availH);
    scrollOffset_ = std::clamp(scrollOffset_, 0.0f, maxScroll_);

    // Scrollbar indicator
    if (maxScroll_ > 0.0f) {
        float scrollbarX = drawerBounds_.x + drawerBounds_.w - 5.0f;
        float thumbH = std::max(24.0f, availH * (availH / (availH + maxScroll_)));
        float thumbY = contentY + (scrollOffset_ / maxScroll_) * (availH - thumbH);
        drawRoundedRect(r, scrollbarX, contentY, 3.0f, availH, 1.5f, 0.0f, 0.0f, 0.0f, 0.18f);
        drawRoundedRect(r, scrollbarX, thumbY, 3.0f, thumbH, 1.5f, theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.55f);
    }

    // -------------------------------------------------------------
    // TAB BODY RENDERING
    // -------------------------------------------------------------
    switch (activeTab_) {
        // --- TAB 1: ASSETS ---
        case BrowserDrawerTab::Assets: {
            float cardH = 40.0f;
            for (size_t i = 0; i < tracks_.size(); ++i) {
                float cy = contentY + static_cast<float>(i) * (cardH + 6.0f) - scrollOffset_;
                if (cy + cardH < contentY || cy > contentY + availH) continue;

                const auto& trk = tracks_[i];
                bool isSel = (static_cast<int>(i) == selectedIndex_);

                drawRoundedRect(r, drawerBounds_.x + 12.0f, cy, drawerBounds_.w - 24.0f, cardH, 4.0f,
                                isSel ? theme.panelHeader.r * 1.3f : theme.panelHeader.r,
                                isSel ? theme.panelHeader.g * 1.3f : theme.panelHeader.g,
                                isSel ? theme.panelHeader.b * 1.3f : theme.panelHeader.b, 0.85f);

                if (isSel) {
                    drawRoundedRectOutline(r, drawerBounds_.x + 12.0f, cy, drawerBounds_.w - 24.0f, cardH, 4.0f,
                                           theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.9f, 1.0f);
                }

                // Track Color Indicator Dot
                drawCircle(r, drawerBounds_.x + 22.0f, cy + 20.0f, 4.0f, trk.color);

                // Track Name & Type Subtitle
                drawText(r, trk.name, drawerBounds_.x + 34.0f, cy + 6.0f, 10.5f,
                         isSel ? theme.primaryAccent.r : theme.textPrimary.r,
                         isSel ? theme.primaryAccent.g : theme.textPrimary.g,
                         isSel ? theme.primaryAccent.b : theme.textPrimary.b, 1.0f);

                std::string sub = trk.type + " • " + std::to_string(trk.clipCount) + " clip(s)";
                drawText(r, sub, drawerBounds_.x + 34.0f, cy + 22.0f, 9.0f,
                         theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.8f);

                // Mute / Solo badges
                if (trk.isMuted) {
                    drawText(r, "MUTE", drawerBounds_.x + drawerBounds_.w - 68.0f, cy + 12.0f, 8.5f, theme.muteActive);
                } else if (trk.isSolo) {
                    drawText(r, "SOLO", drawerBounds_.x + drawerBounds_.w - 68.0f, cy + 12.0f, 8.5f, theme.soloActive);
                }
            }
            break;
        }

        // --- TAB 2: SCRIPTS ---
        case BrowserDrawerTab::Scripts: {
            float cardH = 46.0f;
            size_t visIdx = 0;
            for (size_t i = 0; i < scripts_.size(); ++i) {
                const auto& item = scripts_[i];
                if (selectedScriptCategory_ != "ALL") {
                    if (selectedScriptCategory_ == "SYNTH" && item.category != "SYNTH") continue;
                    if (selectedScriptCategory_ == "AUDIO_FX" && item.category != "AUDIO_FX") continue;
                    if (selectedScriptCategory_ == "MIDI_FX" && item.category != "MIDI_FX") continue;
                    if (selectedScriptCategory_ == "MIDI_SEQ" && item.category != "MIDI_SEQ") continue;
                    if (selectedScriptCategory_ == "MACRO" && item.category != "MACRO") continue;
                }
                if (!searchQuery_.empty()) {
                    std::string q = searchQuery_;
                    std::transform(q.begin(), q.end(), q.begin(), ::tolower);
                    std::string nameLower = item.name;
                    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                    std::string catLower = item.category;
                    std::transform(catLower.begin(), catLower.end(), catLower.begin(), ::tolower);
                    std::string descLower = item.description;
                    std::transform(descLower.begin(), descLower.end(), descLower.begin(), ::tolower);
                    if (nameLower.find(q) == std::string::npos && catLower.find(q) == std::string::npos && descLower.find(q) == std::string::npos) {
                        continue;
                    }
                }

                float cy = contentY + static_cast<float>(visIdx) * (cardH + 6.0f) - scrollOffset_;
                visIdx++;
                if (cy + cardH < contentY || cy > contentY + availH) continue;

                bool isSel = (static_cast<int>(i) == selectedIndex_);
                Color badgeCol = theme.secondaryAccent;
                const char* iconRef = "ui_cat_all";
                if (item.category == "SYNTH") {
                    badgeCol = Color(1.0f, 0.55f, 0.0f, 1.0f);
                    iconRef = "ui_cat_instruments";
                } else if (item.category == "AUDIO_FX") {
                    badgeCol = Color(0.13f, 0.96f, 0.91f, 1.0f);
                    iconRef = "ui_cat_fx";
                } else if (item.category == "MIDI_FX") {
                    badgeCol = Color(0.0f, 1.0f, 0.4f, 1.0f);
                    iconRef = "ui_cat_midifx";
                } else if (item.category == "MIDI_SEQ") {
                    badgeCol = Color(1.0f, 0.84f, 0.0f, 1.0f);
                    iconRef = "ui_cat_seq";
                } else if (item.category == "MACRO") {
                    badgeCol = Color(0.74f, 0.0f, 1.0f, 1.0f);
                    iconRef = "ui_cat_macro";
                }

                drawRoundedRect(r, drawerBounds_.x + 12.0f, cy, drawerBounds_.w - 24.0f, cardH, 4.0f,
                                isSel ? theme.panelHeader.r * 1.3f : theme.panelHeader.r,
                                isSel ? theme.panelHeader.g * 1.3f : theme.panelHeader.g,
                                isSel ? theme.panelHeader.b * 1.3f : theme.panelHeader.b, 0.85f);

                if (isSel) {
                    drawRoundedRectOutline(r, drawerBounds_.x + 12.0f, cy, drawerBounds_.w - 24.0f, cardH, 4.0f,
                                           theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.9f, 1.0f);
                }

                // Render category mini icon
                IconRegistry::instance().renderIcon(r, iconRef, Rect2D(drawerBounds_.x + 18.0f, cy + 8.0f, 13.0f, 13.0f), badgeCol);

                drawText(r, item.name, drawerBounds_.x + 36.0f, cy + 6.0f, 10.5f,
                         isSel ? theme.primaryAccent.r : theme.textPrimary.r,
                         isSel ? theme.primaryAccent.g : theme.textPrimary.g,
                         isSel ? theme.primaryAccent.b : theme.textPrimary.b, 1.0f);

                drawText(r, item.description, drawerBounds_.x + 36.0f, cy + 24.0f, 9.0f,
                         theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.8f);

                float tagW = 56.0f;
                float tagX = drawerBounds_.x + drawerBounds_.w - 24.0f - tagW;
                drawRoundedRect(r, tagX, cy + 6.0f, tagW, 16.0f, 2.0f,
                                badgeCol.r * 0.2f, badgeCol.g * 0.2f, badgeCol.b * 0.2f, 0.8f);
                drawCenteredText(r, item.category, tagX, cy + 6.0f, tagW, 16.0f, 8.0f,
                                 badgeCol.r, badgeCol.g, badgeCol.b, 0.95f);
            }
            break;
        }

        // --- TAB 3: PRESETS & SPACES ---
        case BrowserDrawerTab::Presets: {
            float cardH = 46.0f;
            size_t visIdx = 0;
            for (size_t i = 0; i < patches_.size(); ++i) {
                const auto& item = patches_[i];
                if (!searchQuery_.empty()) {
                    std::string q = searchQuery_;
                    std::transform(q.begin(), q.end(), q.begin(), ::tolower);
                    std::string nameLower = item.name;
                    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                    if (nameLower.find(q) == std::string::npos && item.category.find(searchQuery_) == std::string::npos) {
                        continue;
                    }
                }

                float cy = contentY + static_cast<float>(visIdx) * (cardH + 6.0f) - scrollOffset_;
                visIdx++;
                if (cy + cardH < contentY || cy > contentY + availH) continue;

                bool isSel = (static_cast<int>(i) == selectedIndex_);
                drawRoundedRect(r, drawerBounds_.x + 12.0f, cy, kDrawerWidth - 24.0f, cardH, 4.0f,
                                isSel ? theme.panelHeader.r * 1.3f : theme.panelHeader.r,
                                isSel ? theme.panelHeader.g * 1.3f : theme.panelHeader.g,
                                isSel ? theme.panelHeader.b * 1.3f : theme.panelHeader.b, 0.85f);

                if (isSel) {
                    drawRoundedRectOutline(r, drawerBounds_.x + 12.0f, cy, kDrawerWidth - 24.0f, cardH, 4.0f,
                                           theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.9f, 1.0f);
                }

                drawText(r, item.name, drawerBounds_.x + 20.0f, cy + 6.0f, 10.5f,
                         isSel ? theme.primaryAccent.r : theme.textPrimary.r,
                         isSel ? theme.primaryAccent.g : theme.textPrimary.g,
                         isSel ? theme.primaryAccent.b : theme.textPrimary.b, 1.0f);

                std::string desc = item.description;
                if (!item.meta.empty()) desc += " • " + item.meta;
                drawText(r, desc, drawerBounds_.x + 20.0f, cy + 24.0f, 9.0f,
                         theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.8f);

                float tagW = 46.0f;
                float tagX = drawerBounds_.x + kDrawerWidth - 24.0f - tagW;
                drawRoundedRect(r, tagX, cy + 6.0f, tagW, 16.0f, 2.0f,
                                theme.primaryAccent.r * 0.2f, theme.primaryAccent.g * 0.2f, theme.primaryAccent.b * 0.2f, 0.8f);
                drawCenteredText(r, item.category, tagX, cy + 6.0f, tagW, 16.0f, 8.0f,
                                 theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.95f);
            }
            break;
        }

        // --- TAB 4: PACKS & AUDIO TO MIDI ---
        case BrowserDrawerTab::Packs: {
            // Quick Tool Card: Audio to MIDI Converter
            float toolCardH = 50.0f;
            drawRoundedRect(r, drawerBounds_.x + 12.0f, contentY, kDrawerWidth - 24.0f, toolCardH, 6.0f,
                            theme.primaryAccent.r * 0.15f, theme.primaryAccent.g * 0.15f, theme.primaryAccent.b * 0.15f, 0.9f);
            drawRoundedRectOutline(r, drawerBounds_.x + 12.0f, contentY, kDrawerWidth - 24.0f, toolCardH, 6.0f,
                                   theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.6f, 1.0f);

            drawText(r, "AUDIO TO MIDI CONVERTER", drawerBounds_.x + 20.0f, contentY + 8.0f, 10.5f,
                     theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);
            drawText(r, "Transcribe recordings into MIDI clips", drawerBounds_.x + 20.0f, contentY + 26.0f, 9.0f,
                     theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.9f);

            Rect2D launchBtn(drawerBounds_.x + kDrawerWidth - 84.0f, contentY + 12.0f, 60.0f, 24.0f);
            drawButton(r, launchBtn, "LAUNCH", theme.primaryAccent, Color(0, 0, 0, 0), Color(0, 0, 0, 1), 9.0f, 3.0f, 0.0f);

            // SoundFont Packs List
            float listY = contentY + toolCardH + 12.0f;
            drawText(r, "SOUNDFONT EXPANSION PACKS", drawerBounds_.x + 14.0f, listY, 9.5f,
                     theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.9f);

            listY += 16.0f;
            float packH = 46.0f;
            for (size_t i = 1; i < packs_.size(); ++i) {
                float cy = listY + static_cast<float>(i - 1) * (packH + 6.0f) - scrollOffset_;
                if (cy + packH < listY || cy > contentY + availH) continue;

                const auto& pk = packs_[i];
                drawRoundedRect(r, drawerBounds_.x + 12.0f, cy, kDrawerWidth - 24.0f, packH, 4.0f,
                                theme.panelHeader.r, theme.panelHeader.g, theme.panelHeader.b, 0.85f);

                drawText(r, pk.title, drawerBounds_.x + 20.0f, cy + 6.0f, 10.5f,
                         theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);

                std::ostringstream ss;
                ss << pk.description << " (" << std::fixed << std::setprecision(1) << pk.sizeMb << " MB)";
                drawText(r, ss.str(), drawerBounds_.x + 20.0f, cy + 24.0f, 9.0f,
                         theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.8f);

                Rect2D loadBtn(drawerBounds_.x + kDrawerWidth - 80.0f, cy + 10.0f, 56.0f, 24.0f);
                if (pk.isInstalled) {
                    drawButton(r, loadBtn, "LOAD", theme.secondaryAccent * 0.25f, theme.secondaryAccent, theme.secondaryAccent, 8.5f, 3.0f, 1.0f);
                } else {
                    drawButton(r, loadBtn, "GET", theme.panelHeader, theme.borderSubtle, theme.primaryAccent, 8.5f, 3.0f, 1.0f);
                }
            }
            break;
        }

        // --- TAB 5: LOCAL SAVED PROJECTS ---
        case BrowserDrawerTab::Projects: {
            // Action Toolbar (SAVE CURRENT, SAVE AS, OPEN FOLDER, REFRESH)
            float actW = (drawerBounds_.w - 36.0f) / 4.0f;
            Rect2D saveCurRect(drawerBounds_.x + 12.0f, contentY, actW, 24.0f);
            Rect2D saveAsRect(drawerBounds_.x + 12.0f + actW + 4.0f, contentY, actW, 24.0f);
            Rect2D folderRect(drawerBounds_.x + 12.0f + (actW + 4.0f) * 2.0f, contentY, actW, 24.0f);
            Rect2D refreshRect(drawerBounds_.x + 12.0f + (actW + 4.0f) * 3.0f, contentY, actW, 24.0f);

            drawButton(r, saveCurRect, "SAVE", theme.primaryAccent * 0.25f, theme.primaryAccent, theme.primaryAccent, 8.5f, 3.0f, 1.0f);
            drawButton(r, saveAsRect, "SAVE AS", theme.secondaryAccent * 0.25f, theme.secondaryAccent, theme.secondaryAccent, 8.0f, 3.0f, 1.0f);
            drawButton(r, folderRect, "EXPLORE", theme.panelHeader, theme.borderSubtle, theme.textPrimary, 8.0f, 3.0f, 1.0f);
            drawButton(r, refreshRect, "REFRESH", theme.panelHeader, theme.borderSubtle, theme.textMuted, 8.0f, 3.0f, 1.0f);

            float listY = contentY + 32.0f;
            float cardH = 50.0f;
            size_t visIdx = 0;

            for (size_t i = 0; i < savedProjects_.size(); ++i) {
                const auto& proj = savedProjects_[i];
                if (!searchQuery_.empty()) {
                    std::string q = searchQuery_;
                    std::transform(q.begin(), q.end(), q.begin(), ::tolower);
                    std::string nameLower = proj.name;
                    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                    if (nameLower.find(q) == std::string::npos && proj.fileName.find(q) == std::string::npos) {
                        continue;
                    }
                }

                float cy = listY + static_cast<float>(visIdx) * (cardH + 6.0f) - scrollOffset_;
                visIdx++;
                if (cy + cardH < listY || cy > contentY + availH) continue;

                drawRoundedRect(r, drawerBounds_.x + 12.0f, cy, kDrawerWidth - 24.0f, cardH, 4.0f,
                                theme.panelHeader.r, theme.panelHeader.g, theme.panelHeader.b, 0.85f);

                drawText(r, proj.name, drawerBounds_.x + 20.0f, cy + 6.0f, 10.5f,
                         theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);

                std::string metaStr = formatBytes(proj.sizeBytes) + " • " + proj.lastModified;
                drawText(r, metaStr, drawerBounds_.x + 20.0f, cy + 22.0f, 8.5f,
                         theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.8f);

                drawText(r, proj.filePath, drawerBounds_.x + 20.0f, cy + 34.0f, 7.5f,
                         theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.6f);

                // Load button on the card
                Rect2D loadBtn(drawerBounds_.x + kDrawerWidth - 78.0f, cy + 12.0f, 54.0f, 26.0f);
                drawButton(r, loadBtn, "LOAD", theme.primaryAccent * 0.25f, theme.primaryAccent, theme.primaryAccent, 9.0f, 3.0f, 1.0f);
            }
            break;
        }

        // --- TAB 6: HISTORY & TIME TRAVEL ---
        case BrowserDrawerTab::History: {
            float cardH = 38.0f;
            for (size_t i = 0; i < history_.size(); ++i) {
                float cy = contentY + static_cast<float>(i) * (cardH + 6.0f) - scrollOffset_;
                if (cy + cardH < contentY || cy > contentY + availH) continue;

                const auto& item = history_[i];
                drawRoundedRect(r, drawerBounds_.x + 12.0f, cy, kDrawerWidth - 24.0f, cardH, 4.0f,
                                item.isCurrent ? theme.primaryAccent.r * 0.2f : theme.panelHeader.r,
                                item.isCurrent ? theme.primaryAccent.g * 0.2f : theme.panelHeader.g,
                                item.isCurrent ? theme.primaryAccent.b * 0.2f : theme.panelHeader.b, 0.85f);

                if (item.isCurrent) {
                    drawRoundedRectOutline(r, drawerBounds_.x + 12.0f, cy, kDrawerWidth - 24.0f, cardH, 4.0f,
                                           theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.9f, 1.0f);
                }

                // Step index circle
                drawCircle(r, drawerBounds_.x + 22.0f, cy + 19.0f, 3.5f,
                           item.isCurrent ? theme.primaryAccent : (item.isMilestone ? theme.secondaryAccent : theme.textMuted));

                drawText(r, item.description, drawerBounds_.x + 34.0f, cy + 6.0f, 10.0f,
                         item.isCurrent ? theme.primaryAccent.r : theme.textPrimary.r,
                         item.isCurrent ? theme.primaryAccent.g : theme.textPrimary.g,
                         item.isCurrent ? theme.primaryAccent.b : theme.textPrimary.b, 1.0f);

                std::string info = item.category + " • Step #" + std::to_string(item.stepIndex);
                drawText(r, info, drawerBounds_.x + 34.0f, cy + 20.0f, 8.5f,
                         theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.8f);

                if (item.isCurrent) {
                    drawText(r, "NOW", drawerBounds_.x + kDrawerWidth - 58.0f, cy + 12.0f, 8.5f,
                             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);
                }
            }
            break;
        }
    }
}

bool ProjectBrowserDrawer::handlePointer(const PointerEvent& ev) {
    if (animProgress_ <= 0.05f) return false;
    mouseX_ = ev.x;
    mouseY_ = ev.y;

    if (ev.action == PointerAction::Up || ev.action == PointerAction::Cancel) {
        isDraggingScroll_ = false;
    }

    if (ev.action == PointerAction::Move && isDraggingScroll_) {
        float dy = ev.y - dragStartY_;
        float maxSc = computeMaxScroll();
        scrollOffset_ = std::clamp(dragStartOffset_ - dy, 0.0f, maxSc);
        return true;
    }

    if (!drawerBounds_.contains(ev.x, ev.y)) return false;

    if (ev.action == PointerAction::Move) {
        return true;
    }

    if (ev.action == PointerAction::Down) {
        // 1. Close button
        if (closeBtnBounds_.contains(ev.x, ev.y)) {
            close();
            return true;
        }

        // 2. Tab strip buttons
        for (size_t i = 0; i < 6; ++i) {
            if (tabBounds_[i].contains(ev.x, ev.y)) {
                setTab(static_cast<BrowserDrawerTab>(i));
                return true;
            }
        }

        // 3. Script category filter chips (when on Scripts tab)
        if (activeTab_ == BrowserDrawerTab::Scripts) {
            const char* catFilters[6] = {"ALL", "SYNTH", "AUDIO_FX", "MIDI_FX", "MIDI_SEQ", "MACRO"};
            for (size_t i = 0; i < 6; ++i) {
                if (scriptCategoryBounds_[i].contains(ev.x, ev.y)) {
                    setScriptCategoryFilter(catFilters[i]);
                    return true;
                }
            }
        }

        // 4. History tab toolbar inside searchBoxBounds_
        if (activeTab_ == BrowserDrawerTab::History) {
            float btnW = (searchBoxBounds_.w - 18.0f) / 4.0f;
            Rect2D undoRect(searchBoxBounds_.x, searchBoxBounds_.y, btnW, 24.0f);
            Rect2D redoRect(searchBoxBounds_.x + btnW + 6.0f, searchBoxBounds_.y, btnW, 24.0f);
            Rect2D checkptRect(searchBoxBounds_.x + (btnW + 6.0f) * 2.0f, searchBoxBounds_.y, btnW, 24.0f);
            Rect2D clearRect(searchBoxBounds_.x + (btnW + 6.0f) * 3.0f, searchBoxBounds_.y, btnW, 24.0f);

            if (undoRect.contains(ev.x, ev.y)) {
                if (onUndo) onUndo();
                return true;
            }
            if (redoRect.contains(ev.x, ev.y)) {
                if (onRedo) onRedo();
                return true;
            }
            if (checkptRect.contains(ev.x, ev.y)) {
                if (onCreateCheckpoint) onCreateCheckpoint("Milestone");
                return true;
            }
            if (clearRect.contains(ev.x, ev.y)) {
                if (onClearHistory) onClearHistory();
                return true;
            }
        }

        // 5. Search box bounds (do not drag scroll when clicked)
        if (searchBoxBounds_.contains(ev.x, ev.y)) {
            return true;
        }

        float contentY = (activeTab_ == BrowserDrawerTab::Scripts) ?
                         (scriptCategoryBounds_[0].y + scriptCategoryBounds_[0].h + 8.0f) :
                         (searchBoxBounds_.y + searchBoxBounds_.h + 8.0f);

        // 6. Tab-specific interactive elements
        switch (activeTab_) {
            case BrowserDrawerTab::Assets: {
                float cardH = 40.0f;
                for (size_t i = 0; i < tracks_.size(); ++i) {
                    float cy = contentY + static_cast<float>(i) * (cardH + 6.0f) - scrollOffset_;
                    Rect2D card(drawerBounds_.x + 12.0f, cy, drawerBounds_.w - 24.0f, cardH);
                    if (card.contains(ev.x, ev.y)) {
                        selectedIndex_ = static_cast<int>(i);
                        if (onSelectTrack) onSelectTrack(tracks_[i].index);
                        return true;
                    }
                }
                break;
            }

            case BrowserDrawerTab::Scripts: {
                float cardH = 46.0f;
                size_t visIdx = 0;
                for (size_t i = 0; i < scripts_.size(); ++i) {
                    const auto& item = scripts_[i];
                    if (selectedScriptCategory_ != "ALL") {
                        if (selectedScriptCategory_ == "SYNTH" && item.category != "SYNTH") continue;
                        if (selectedScriptCategory_ == "AUDIO_FX" && item.category != "AUDIO_FX") continue;
                        if (selectedScriptCategory_ == "MIDI_FX" && item.category != "MIDI_FX") continue;
                        if (selectedScriptCategory_ == "MIDI_SEQ" && item.category != "MIDI_SEQ") continue;
                        if (selectedScriptCategory_ == "MACRO" && item.category != "MACRO") continue;
                    }
                    if (!searchQuery_.empty()) {
                        std::string q = searchQuery_;
                        std::transform(q.begin(), q.end(), q.begin(), ::tolower);
                        std::string nameLower = item.name;
                        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                        std::string catLower = item.category;
                        std::transform(catLower.begin(), catLower.end(), catLower.begin(), ::tolower);
                        std::string descLower = item.description;
                        std::transform(descLower.begin(), descLower.end(), descLower.begin(), ::tolower);
                        if (nameLower.find(q) == std::string::npos && catLower.find(q) == std::string::npos && descLower.find(q) == std::string::npos) {
                            continue;
                        }
                    }

                    float cy = contentY + static_cast<float>(visIdx) * (cardH + 6.0f) - scrollOffset_;
                    visIdx++;
                    Rect2D card(drawerBounds_.x + 12.0f, cy, drawerBounds_.w - 24.0f, cardH);
                    if (card.contains(ev.x, ev.y)) {
                        selectedIndex_ = static_cast<int>(i);
                        if (item.category == "SYNTH" && onAddPresetTrack) {
                            onAddPresetTrack(item.id);
                        } else if (item.category == "AUDIO_FX" && onAddAudioFx) {
                            onAddAudioFx(item.id);
                        } else if (item.category == "MIDI_FX" && onAddMidiFx) {
                            onAddMidiFx(item.id);
                        } else if (onRunScript) {
                            onRunScript(item.id);
                        }
                        return true;
                    }
                }
                break;
            }

            case BrowserDrawerTab::Presets: {
                float cardH = 46.0f;
                size_t visIdx = 0;
                for (size_t i = 0; i < patches_.size(); ++i) {
                    const auto& item = patches_[i];
                    if (!searchQuery_.empty()) {
                        std::string q = searchQuery_;
                        std::transform(q.begin(), q.end(), q.begin(), ::tolower);
                        std::string nameLower = item.name;
                        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                        if (nameLower.find(q) == std::string::npos && item.category.find(searchQuery_) == std::string::npos) {
                            continue;
                        }
                    }

                    float cy = contentY + static_cast<float>(visIdx) * (cardH + 6.0f) - scrollOffset_;
                    visIdx++;
                    Rect2D card(drawerBounds_.x + 12.0f, cy, drawerBounds_.w - 24.0f, cardH);
                    if (card.contains(ev.x, ev.y)) {
                        selectedIndex_ = static_cast<int>(i);
                        if (onSelectPreset) onSelectPreset(item.id);
                        return true;
                    }
                }
                break;
            }

            case BrowserDrawerTab::Packs: {
                // Quick Launch Card
                float toolCardH = 50.0f;
                Rect2D launchBtn(drawerBounds_.x + drawerBounds_.w - 84.0f, contentY + 12.0f, 60.0f, 24.0f);
                if (launchBtn.contains(ev.x, ev.y)) {
                    if (onLaunchAudioToMidi) onLaunchAudioToMidi();
                    return true;
                }

                // SoundFont cards
                float listY = contentY + toolCardH + 28.0f;
                float packH = 46.0f;
                for (size_t i = 1; i < packs_.size(); ++i) {
                    float cy = listY + static_cast<float>(i - 1) * (packH + 6.0f) - scrollOffset_;
                    Rect2D card(drawerBounds_.x + 12.0f, cy, drawerBounds_.w - 24.0f, packH);
                    if (card.contains(ev.x, ev.y)) {
                        if (onSelectPreset) onSelectPreset(packs_[i].id);
                        return true;
                    }
                }
                break;
            }

            case BrowserDrawerTab::Projects: {
                // Toolbar (SAVE, SAVE AS, OPEN FOLDER, REFRESH)
                float actW = (drawerBounds_.w - 36.0f) / 4.0f;
                Rect2D saveCurRect(drawerBounds_.x + 12.0f, contentY, actW, 24.0f);
                Rect2D saveAsRect(drawerBounds_.x + 12.0f + actW + 4.0f, contentY, actW, 24.0f);
                Rect2D folderRect(drawerBounds_.x + 12.0f + (actW + 4.0f) * 2.0f, contentY, actW, 24.0f);
                Rect2D refreshRect(drawerBounds_.x + 12.0f + (actW + 4.0f) * 3.0f, contentY, actW, 24.0f);

                if (saveCurRect.contains(ev.x, ev.y)) {
                    if (onSaveProject) onSaveProject("./Projects/my_song.eats");
                    return true;
                }
                if (saveAsRect.contains(ev.x, ev.y)) {
                    if (onSaveProjectAs) onSaveProjectAs();
                    return true;
                }
                if (folderRect.contains(ev.x, ev.y)) {
                    if (onOpenProjectsFolder) onOpenProjectsFolder();
                    return true;
                }
                if (refreshRect.contains(ev.x, ev.y)) {
                    scanSavedProjects();
                    return true;
                }

                // Project Cards [LOAD]
                float listY = contentY + 32.0f;
                float cardH = 50.0f;
                size_t visIdx = 0;
                for (size_t i = 0; i < savedProjects_.size(); ++i) {
                    const auto& proj = savedProjects_[i];
                    if (!searchQuery_.empty()) {
                        std::string q = searchQuery_;
                        std::transform(q.begin(), q.end(), q.begin(), ::tolower);
                        std::string nameLower = proj.name;
                        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                        if (nameLower.find(q) == std::string::npos && proj.fileName.find(q) == std::string::npos) {
                            continue;
                        }
                    }

                    float cy = listY + static_cast<float>(visIdx) * (cardH + 6.0f) - scrollOffset_;
                    visIdx++;
                    Rect2D card(drawerBounds_.x + 12.0f, cy, drawerBounds_.w - 24.0f, cardH);
                    if (card.contains(ev.x, ev.y)) {
                        if (onLoadProject) onLoadProject(proj.filePath);
                        return true;
                    }
                }
                break;
            }

            case BrowserDrawerTab::History: {
                // Milestone rows
                float cardH = 38.0f;
                for (size_t i = 0; i < history_.size(); ++i) {
                    float cy = contentY + static_cast<float>(i) * (cardH + 6.0f) - scrollOffset_;
                    Rect2D card(drawerBounds_.x + 12.0f, cy, drawerBounds_.w - 24.0f, cardH);
                    if (card.contains(ev.x, ev.y)) {
                        if (onJumpToHistory) onJumpToHistory(history_[i].stepIndex);
                        return true;
                    }
                }
                break;
            }
        }

        // Only start drag scrolling if click is inside the scrollable content area and didn't hit any interactive card/button
        if (ev.y >= contentY) {
            isDraggingScroll_ = true;
            dragStartY_ = ev.y;
            dragStartOffset_ = scrollOffset_;
        }
        return true;
    } else if (ev.action == PointerAction::Scroll) {
        float maxSc = computeMaxScroll();
        scrollOffset_ -= ev.scrollY * 24.0f;
        scrollOffset_ = std::clamp(scrollOffset_, 0.0f, maxSc);
        return true;
    }

    return true;
}

} // namespace eatsbits::ui
