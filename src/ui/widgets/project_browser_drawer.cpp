#include "eatsbits/ui/widgets/project_browser_drawer.hpp"
#include "eatsbits/ui/draw_utils.hpp"
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
    if (tab == BrowserDrawerTab::Projects) {
        scanSavedProjects();
    }
}

void ProjectBrowserDrawer::setTracks(const std::vector<BrowserTrackAssetItem>& tracks) {
    tracks_ = tracks;
}

void ProjectBrowserDrawer::setHistory(const std::vector<BrowserHistoryMilestoneItem>& history, bool canUndo, bool canRedo) {
    history_ = history;
    canUndo_ = canUndo;
    canRedo_ = canRedo;
}

void ProjectBrowserDrawer::layout(float screenWidth, float screenHeight, float topHeaderHeight, float bottomNavHeight) {
    float drawerH = screenHeight - topHeaderHeight - bottomNavHeight;
    float currentX = screenWidth - (kDrawerWidth * animProgress_);

    drawerBounds_ = Rect2D(currentX, topHeaderHeight, kDrawerWidth, drawerH);

    float headerY = topHeaderHeight + 8.0f;
    closeBtnBounds_ = Rect2D(currentX + kDrawerWidth - 32.0f, headerY, 24.0f, 24.0f);

    // 6 Tabs layout
    float tabY = headerY + 30.0f;
    float totalTabW = kDrawerWidth - 24.0f;
    float tabW = totalTabW / 6.0f;

    for (size_t i = 0; i < 6; ++i) {
        tabBounds_[i] = Rect2D(currentX + 12.0f + (static_cast<float>(i) * tabW), tabY, tabW - 2.0f, 22.0f);
    }

    // Search bar below tab strip
    float subY = tabY + 28.0f;
    searchBoxBounds_ = Rect2D(currentX + 12.0f, subY, kDrawerWidth - 24.0f, 24.0f);
}

void ProjectBrowserDrawer::update(float dt) {
    float target = isOpen_ ? 1.0f : 0.0f;
    float speed = 28.0f;
    animProgress_ += (target - animProgress_) * std::clamp(dt * speed, 0.0f, 1.0f);

    if (std::abs(animProgress_ - target) < 0.005f) {
        animProgress_ = target;
    }

    animOffset_ = kDrawerWidth * (1.0f - animProgress_);
}

void ProjectBrowserDrawer::render(BatchRenderer2D& r, const ThemeTokens& theme) {
    if (animProgress_ <= 0.001f) return;

    // Semi-transparent backdrop shadow
    if (isOpen_ && animProgress_ > 0.05f) {
        float shadowAlpha = 0.40f * animProgress_;
        drawRect(r, 0.0f, drawerBounds_.y, drawerBounds_.x, drawerBounds_.h,
                 0.0f, 0.0f, 0.0f, shadowAlpha);
    }

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

    // 6 Tab Strip
    const char* tabNames[6] = {"ASSETS", "SCRIPT", "PRESET", "PACKS", "PROJ", "HIST"};
    for (size_t i = 0; i < 6; ++i) {
        bool active = (static_cast<size_t>(activeTab_) == i);
        if (active) {
            Color bg{theme.primaryAccent.r * 0.25f, theme.primaryAccent.g * 0.25f, theme.primaryAccent.b * 0.25f, 0.95f};
            drawButton(r, tabBounds_[i], tabNames[i], bg, theme.primaryAccent, theme.primaryAccent, 8.5f, 3.0f, 1.0f);
        } else {
            drawButton(r, tabBounds_[i], tabNames[i], theme.panelHeader, Color(0.0f, 0.0f, 0.0f, 0.0f), theme.textMuted, 8.5f, 3.0f, 0.0f);
        }
    }

    float contentY = searchBoxBounds_.y + searchBoxBounds_.h + 8.0f;
    float availH = drawerBounds_.y + drawerBounds_.h - contentY - 8.0f;

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

                drawRoundedRect(r, drawerBounds_.x + 12.0f, cy, kDrawerWidth - 24.0f, cardH, 4.0f,
                                isSel ? theme.panelHeader.r * 1.3f : theme.panelHeader.r,
                                isSel ? theme.panelHeader.g * 1.3f : theme.panelHeader.g,
                                isSel ? theme.panelHeader.b * 1.3f : theme.panelHeader.b, 0.85f);

                if (isSel) {
                    drawRoundedRectOutline(r, drawerBounds_.x + 12.0f, cy, kDrawerWidth - 24.0f, cardH, 4.0f,
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
                    drawText(r, "MUTE", drawerBounds_.x + kDrawerWidth - 68.0f, cy + 12.0f, 8.5f, theme.muteActive);
                } else if (trk.isSolo) {
                    drawText(r, "SOLO", drawerBounds_.x + kDrawerWidth - 68.0f, cy + 12.0f, 8.5f, theme.soloActive);
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

                drawText(r, item.description, drawerBounds_.x + 20.0f, cy + 24.0f, 9.0f,
                         theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.8f);

                float tagW = 56.0f;
                float tagX = drawerBounds_.x + kDrawerWidth - 24.0f - tagW;
                drawRoundedRect(r, tagX, cy + 6.0f, tagW, 16.0f, 2.0f,
                                theme.secondaryAccent.r * 0.2f, theme.secondaryAccent.g * 0.2f, theme.secondaryAccent.b * 0.2f, 0.8f);
                drawCenteredText(r, item.category, tagX, cy + 6.0f, tagW, 16.0f, 8.0f,
                                 theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b, 0.95f);
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

    if (ev.action == PointerAction::Move && isDraggingScroll_) {
        float dy = ev.y - dragStartY_;
        scrollOffset_ = std::max(0.0f, dragStartOffset_ - dy);
        return true;
    }

    if (ev.action == PointerAction::Up || ev.action == PointerAction::Cancel) {
        isDraggingScroll_ = false;
    }

    // Click outside drawer -> close
    if (isOpen_ && ev.action == PointerAction::Down && ev.x < drawerBounds_.x) {
        close();
        return true;
    }

    if (!drawerBounds_.contains(ev.x, ev.y)) return false;

    if (ev.action == PointerAction::Move) {
        return true;
    }

    if (ev.action == PointerAction::Down) {
        isDraggingScroll_ = true;
        dragStartY_ = ev.y;
        dragStartOffset_ = scrollOffset_;
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

        float contentY = searchBoxBounds_.y + searchBoxBounds_.h + 8.0f;

        // 3. Tab-specific interactive elements
        switch (activeTab_) {
            case BrowserDrawerTab::Assets: {
                float cardH = 40.0f;
                for (size_t i = 0; i < tracks_.size(); ++i) {
                    float cy = contentY + static_cast<float>(i) * (cardH + 6.0f) - scrollOffset_;
                    Rect2D card(drawerBounds_.x + 12.0f, cy, kDrawerWidth - 24.0f, cardH);
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
                    Rect2D card(drawerBounds_.x + 12.0f, cy, kDrawerWidth - 24.0f, cardH);
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
                    Rect2D card(drawerBounds_.x + 12.0f, cy, kDrawerWidth - 24.0f, cardH);
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
                Rect2D launchBtn(drawerBounds_.x + kDrawerWidth - 84.0f, contentY + 12.0f, 60.0f, 24.0f);
                if (launchBtn.contains(ev.x, ev.y)) {
                    if (onLaunchAudioToMidi) onLaunchAudioToMidi();
                    return true;
                }

                // SoundFont cards
                float listY = contentY + toolCardH + 28.0f;
                float packH = 46.0f;
                for (size_t i = 1; i < packs_.size(); ++i) {
                    float cy = listY + static_cast<float>(i - 1) * (packH + 6.0f) - scrollOffset_;
                    Rect2D card(drawerBounds_.x + 12.0f, cy, kDrawerWidth - 24.0f, packH);
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
                    Rect2D card(drawerBounds_.x + 12.0f, cy, kDrawerWidth - 24.0f, cardH);
                    if (card.contains(ev.x, ev.y)) {
                        if (onLoadProject) onLoadProject(proj.filePath);
                        return true;
                    }
                }
                break;
            }

            case BrowserDrawerTab::History: {
                // Toolbar (UNDO, REDO, +PIN, CLEAR)
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

                // Milestone rows
                float cardH = 38.0f;
                for (size_t i = 0; i < history_.size(); ++i) {
                    float cy = contentY + static_cast<float>(i) * (cardH + 6.0f) - scrollOffset_;
                    Rect2D card(drawerBounds_.x + 12.0f, cy, kDrawerWidth - 24.0f, cardH);
                    if (card.contains(ev.x, ev.y)) {
                        if (onJumpToHistory) onJumpToHistory(history_[i].stepIndex);
                        return true;
                    }
                }
                break;
            }
        }
    } else if (ev.action == PointerAction::Scroll) {
        scrollOffset_ -= ev.scrollY * 24.0f;
        scrollOffset_ = std::max(0.0f, scrollOffset_);
        return true;
    }

    return true;
}

} // namespace eatsbits::ui
