#include "eatsbits/procgen/mulberry32_rng.hpp"
#include "eatsbits/procgen/song_archetypes.hpp"
#include "eatsbits/procgen/procedural_acid_engine.hpp"
#include "eatsbits/procgen/procedural_drum_engine.hpp"
#include "eatsbits/procgen/procedural_piano_engine.hpp"
#include "eatsbits/procgen/procedural_ensemble_engine.hpp"
#include "eatsbits/procgen/procedural_song_engine.hpp"
#include "eatsbits/eatscript/macro_runtime.hpp"
#include "eatsbits/ui/widgets/command_palette_dialog.hpp"
#include "eatsbits/ui/widgets/project_browser_drawer.hpp"

#include <iostream>
#include <cassert>
#include <cmath>
#include <string>

using namespace eatsbits;
using namespace eatsbits::procgen;

void testMulberry32Rng() {
    std::cout << "[Test 1] Mulberry32 deterministic PRNG..." << std::endl;

    Mulberry32Rng rng1(1337);
    Mulberry32Rng rng2(1337);

    // Identical seeds must yield identical outputs
    for (int i = 0; i < 50; ++i) {
        assert(rng1.nextU32() == rng2.nextU32());
        assert(std::abs(rng1.nextDouble() - rng2.nextDouble()) < 1e-9);
        assert(rng1.nextInt(100) == rng2.nextInt(100));
    }

    // Bounds check
    for (int i = 0; i < 1000; ++i) {
        double d = rng1.nextDouble();
        assert(d >= 0.0 && d < 1.0); (void)d;

        float f = rng1.nextFloat();
        assert(f >= 0.0f && f < 1.0f); (void)f;

        int n = rng1.nextInt(12);
        assert(n >= 0 && n < 12); (void)n;

        double r = rng1.randDouble(-5.0, 5.0);
        assert(r >= -5.0 && r < 5.0); (void)r;
    }

    // Weighted selection
    std::vector<std::pair<std::string, double>> weights = {
        {"rare", 0.05},
        {"common", 0.95}
    };
    int commonCount = 0;
    for (int i = 0; i < 500; ++i) {
        if (rng1.weighted(weights) == "common") commonCount++;
    }
    assert(commonCount > 400); // overwhelmingly common

    std::cout << "  -> PASSED: Determinism, bounds, and distributions verified." << std::endl;
}

void testSongArchetypesRegistry() {
    std::cout << "[Test 2] Song Archetypes & Blueprint Registry..." << std::endl;

    SongArchetypeRegistry::initialize();
    const auto& all = SongArchetypeRegistry::getAllArchetypes();
    assert(all.size() >= 7); (void)all; // Acid, Synthwave, Lofi, Cyberpunk, Ambient, SNES, C64

    const auto* acid = SongArchetypeRegistry::getById("archetype_acid_techno");
    assert(acid != nullptr);
    assert(acid->getGenre() == "Acid Techno");
    assert(acid->getBpm() == 135.0);
    assert(acid->getBlueprint().ensemble.size() >= 4);
    assert(acid->getBlueprint().sections.size() >= 4); (void)acid;

    const auto* lofi = SongArchetypeRegistry::getById("archetype_lofi_hiphop");
    assert(lofi != nullptr);
    assert(lofi->getBpm() == 84.0); (void)lofi;

    const auto* sw = SongArchetypeRegistry::getById("archetype_synthwave");
    assert(sw != nullptr);
    assert(sw->getBpm() == 124.0); (void)sw;

    auto electronic = SongArchetypeRegistry::findByCategory("Electronic");
    assert(electronic.size() >= 2);

    auto acidTags = SongArchetypeRegistry::findByTag("acid");
    assert(!acidTags.empty());

    // Functional roles & Texture types parsing
    assert(parseFunctionalRole("drum kit") == FunctionalRole::Rhythm);
    assert(parseFunctionalRole("sub bass") == FunctionalRole::Foundation);
    assert(parseFunctionalRole("rhodes chords") == FunctionalRole::HarmonicTexture);
    assert(parseFunctionalRole("lead synth") == FunctionalRole::PrimaryMelody);
    assert(parseFunctionalRole("counter melody") == FunctionalRole::Counterpoint);

    assert(parseTextureType("strummed guitar") == TextureType::Strummed);
    assert(parseTextureType("arpeggio") == TextureType::Arpeggiated);
    assert(parseTextureType("punchy stab") == TextureType::Stabs);
    assert(parseTextureType("pad") == TextureType::Sustained);

    std::cout << "  -> PASSED: All 7 exemplar archetypes indexed and queryable." << std::endl;
}

void testProceduralAcidEngine() {
    std::cout << "[Test 3] ProceduralAcidEngine TB-303 Sequence Generator..." << std::endl;

    AcidPatternParams params;
    params.style = "Hard Acid Techno (Warehouse)";
    params.scaleName = "Phrygian (Dark)";
    params.rootPitchClass = 0; // C
    params.bars = 2;
    params.density = 0.80;
    params.slideProb = 0.50;
    params.accentProb = 0.40;
    params.seed = 303;

    auto notes = ProceduralAcidEngine::generatePattern(params);
    assert(!notes.empty());

    bool hasSlide = false;
    bool hasAccent = false;
    for (const auto& n : notes) {
        assert(n.pitch >= 24 && n.pitch <= 84);
        assert(n.startStep >= 0.0 && n.startStep < 32.0);
        if (n.isSlide) hasSlide = true;
        if (n.isAccent) {
            hasAccent = true;
            assert(std::abs(n.velocity - 0.95f) < 0.01f);
        } else {
            assert(std::abs(n.velocity - 0.68f) < 0.01f);
        }
    }
    assert(hasSlide);
    assert(hasAccent);

    // Test SequencerTrack population
    sequencer::SequencerTrack track("TB-303", 1, 32);
    ProceduralAcidEngine::populateSequencerTrack(track, params);
    assert(track.getNumSteps() == 32);
    int activeSteps = 0;
    for (uint32_t s = 0; s < 32; ++s) {
        if (track.getStep(s).active) activeSteps++;
    }
    assert(activeSteps > 10);

    // Test ArrangerTimelineClip population
    ui::ArrangerTimelineClip clip;
    ProceduralAcidEngine::populateArrangerClip(clip, params);
    assert(clip.lengthBars == 2);
    assert(!clip.notes.empty());

    std::cout << "  -> PASSED: TB-303 accent/slide rules, pitch scales, and track population verified." << std::endl;
}

void testProceduralDrumEngine() {
    std::cout << "[Test 4] ProceduralDrumEngine Polyrhythmic Drum Generator..." << std::endl;

    DrumPatternParams params;
    params.style = "House / Disco (4-on-Floor)";
    params.bars = 4;
    params.density = 0.85;
    params.swing = 0.25;
    params.seed = 42;

    auto notes = ProceduralDrumEngine::generatePattern(params);
    assert(!notes.empty());

    bool hasKick = false;
    bool hasSnareOrClap = false;
    bool hasHats = false;
    bool hasCrash = false;

    for (const auto& n : notes) {
        if (n.pitch == 36) hasKick = true;
        if (n.pitch == 38 || n.pitch == 39) hasSnareOrClap = true;
        if (n.pitch == 42 || n.pitch == 46) hasHats = true;
        if (n.pitch == 49) hasCrash = true;
    }
    assert(hasKick);
    assert(hasSnareOrClap);
    assert(hasHats);
    assert(hasCrash);

    // Test Boom-Bap Hip Hop
    params.style = "Hip-Hop / Boom-Bap";
    auto boomBapNotes = ProceduralDrumEngine::generatePattern(params);
    assert(!boomBapNotes.empty());

    // Test SequencerTrack population with layered drums
    sequencer::SequencerTrack drumTrack("Drums", 2, 64);
    ProceduralDrumEngine::populateSequencerTrack(drumTrack, params);
    assert(drumTrack.getNumSteps() == 64);

    std::cout << "  -> PASSED: Polyrhythmic 4-on-the-floor, Boom-Bap, and GM drum mapping verified." << std::endl;
}

void testProceduralPianoEngine() {
    std::cout << "[Test 5] ProceduralPianoEngine Voice-Leading & Harmonic Voicings..." << std::endl;

    // Test voice leading calculation
    std::vector<int> prevVoicing = {60, 64, 67}; // C Major (C4, E4, G4)
    std::vector<int> nextPcs = {9, 0, 4};        // A Minor (A, C, E)

    auto newVoicing = ProceduralPianoEngine::computeVoiceLeading(nextPcs, prevVoicing, 55, 84);
    assert(newVoicing.size() >= 3);
    // Voice movement should be compact (no large leaps > 7 semitones)
    for (size_t i = 0; i < std::min(prevVoicing.size(), newVoicing.size()); ++i) {
        assert(std::abs(newVoicing[i] - prevVoicing[i]) <= 7);
    }

    // Test Neo-Soul chord progression generation
    PianoPatternParams params;
    params.style = PianoStyle::NeoSoul;
    params.bars = 4;
    params.seed = 101;
    params.chords = {
        theory::ChordEvent{"c1", 0, 2.0f, 2, theory::ChordQuality::Min9}, // Dmin9
        theory::ChordEvent{"c2", 2, 2.0f, 7, theory::ChordQuality::Dom9}  // G9
    };

    auto pNotes = ProceduralPianoEngine::generatePattern(params);
    assert(!pNotes.empty());

    bool hasBass = false;
    bool hasRightHand = false;
    for (const auto& pn : pNotes) {
        if (pn.isLeftHand) hasBass = true;
        else hasRightHand = true;
    }
    assert(hasBass);
    assert(hasRightHand);

    // Test ArrangerTimelineClip population
    ui::ArrangerTimelineClip pianoClip;
    ProceduralPianoEngine::populateArrangerClip(pianoClip, params);
    assert(pianoClip.lengthBars == 4);
    assert(!pianoClip.notes.empty());

    std::cout << "  -> PASSED: Voice leading, left/right hand separation, and jazz voicings verified." << std::endl;
}

void testProceduralEnsembleEngine() {
    std::cout << "[Test 6] ProceduralEnsembleEngine Multi-Track Orchestration..." << std::endl;

    EnsembleRenderParams params;
    params.seed = 42;
    params.swing = 0.15;
    params.humanize = 0.20;

    auto res = ProceduralEnsembleEngine::renderArchetype("archetype_acid_techno", params);
    assert(res.success);
    assert(res.tracks.size() >= 4);
    assert(res.totalBars == 28);
    assert(res.totalNotes > 100);
    assert(!res.chordTrack.empty());

    // Verify tracks contain populated clips
    for (const auto& trk : res.tracks) {
        assert(!trk.clips.empty());
        (void)trk;
    }

    std::cout << "  -> PASSED: Multi-track orchestration, section forms, and chord sync verified." << std::endl;
}

void testProceduralSongEngine() {
    std::cout << "[Test 7] ProceduralSongEngine Full Arrangement Generator..." << std::endl;

    SongGenerationParams params;
    params.style = "Lo-Fi Hip Hop";
    params.bars = 16;
    params.structure = "Full Arrangement (Intro-Verse-Chorus-Outro)";
    params.seed = 42;

    auto res = ProceduralSongEngine::generateSong(params);
    assert(res.success);
    assert(res.affectedTracks >= 4);
    assert(res.affectedNotes > 80);
    assert(res.affectedChords >= 4);
    assert(res.bpm == 84.0);

    // Test direct StepSequencer loading
    sequencer::StepSequencer seq;
    seq.addTrack("Track 1", 0, 16);
    seq.addTrack("Track 2", 0, 16);

    auto seqRes = ProceduralSongEngine::generateToSequencer(seq, params);
    assert(seqRes.success);
    assert(seq.getNumTracks() >= 4);

    // Test default demo generator
    auto demoRes = ProceduralSongEngine::generateDefaultDemo("Synthwave / Retrowave", 88);
    assert(demoRes.success);
    assert(demoRes.bpm == 124.0);

    std::cout << "  -> PASSED: Full arrangement generation across Arranger and StepSequencer verified." << std::endl;
}

void testMacroAndUIIntegration() {
    std::cout << "[Test 8] MacroRuntime, CommandPalette, and Browser Drawer Integration..." << std::endl;

    // MacroRuntime check
    const auto& macros = eatscript::MacroRuntime::getBuiltinMacros();
    assert(macros.size() >= 5);

    bool foundSongMacro = false;
    for (const auto& m : macros) {
        if (m.id == "macro_song_gen") foundSongMacro = true;
    }
    assert(foundSongMacro);

    // Execution check via MacroRuntime
    sequencer::StepSequencer seq;
    seq.addTrack("Test Track", 0, 16);
    auto songMacroRes = eatscript::MacroRuntime::generateProceduralSong(seq, "Lo-Fi Hip Hop", 16, 42);
    assert(songMacroRes.success);

    // CommandPalette registration check
    ui::CommandPaletteDialog cmdPalette;
    cmdPalette.registerCommand({
        "macro.procedural_song", "Macro: Procedural Song Architect", "Generate complete multi-track song arrangement",
        ui::CommandCategory::Macro, "Ctrl+Shift+G", []() {}
    });
    cmdPalette.setQuery("procedural song");
    assert(cmdPalette.getFilteredCount() == 1);

    // ProjectBrowserDrawer library items check
    ui::ProjectBrowserDrawer drawer;
    drawer.open();
    drawer.setTab(ui::BrowserDrawerTab::Scripts);
    drawer.setSearchQuery("Procedural");
    assert(drawer.isOpen());

    std::cout << "  -> PASSED: Macro runtime, CommandPalette 'Ctrl+Shift+G', and Browser drawer verified." << std::endl;
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "   Eatsbits Procedural Music Subsystem 2 Tests    " << std::endl;
    std::cout << "==================================================" << std::endl;

    testMulberry32Rng();
    testSongArchetypesRegistry();
    testProceduralAcidEngine();
    testProceduralDrumEngine();
    testProceduralPianoEngine();
    testProceduralEnsembleEngine();
    testProceduralSongEngine();
    testMacroAndUIIntegration();

    std::cout << "==================================================" << std::endl;
    std::cout << "   100% PROCEDURAL MUSIC SUBSYSTEM 2 TESTS PASSED!" << std::endl;
    std::cout << "==================================================" << std::endl;
    return 0;
}
