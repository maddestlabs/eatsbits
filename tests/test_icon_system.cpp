#include "eatsbits/ui/icon_registry.hpp"
#include "eatsbits/ui/widgets/icon_search_dialog.hpp"
#include "eatsbits/sequencer/step_sequencer.hpp"
#include "eatsbits/project/project_file.hpp"
#include <iostream>
#include <cassert>

using namespace eatsbits;
using namespace eatsbits::ui;

void testIconRegistryBasics() {
    std::cout << "[Test] IconRegistry query & lookup...\n";
    auto& reg = IconRegistry::instance();

    // Verify stock icons
    const auto* synthIcon = reg.findIcon("inst_synth");
    assert(synthIcon != nullptr);
    assert(synthIcon->displayName == "Poly Synth");
    assert(synthIcon->category == "Instruments");

    const auto* kickIcon = reg.findIcon("drum_kick");
    assert(kickIcon != nullptr);
    assert(kickIcon->category == "Drums");

    // Query tests
    auto allIcons = reg.getAllIcons();
    assert(allIcons.size() >= 15);

    auto drumIcons = reg.query("", "Drums");
    assert(!drumIcons.empty());
    for (const auto* ic : drumIcons) {
        assert(ic->category == "Drums");
    }

    auto synthMatches = reg.query("analog", "");
    assert(!synthMatches.empty());

    // Verify all registered icons parse and produce valid non-empty geometry
    for (const auto* ic : allIcons) {
        assert(!ic->id.empty());
        assert(!ic->svgPath.empty());
        SvgPath path = SvgPath::parse(ic->svgPath);
        assert(!path.empty());
        assert(!path.getContours().empty());
        auto tris = path.triangulate();
        assert(!tris.empty());
        assert(path.getBounds().width() > 0.0f);
        assert(path.getBounds().height() > 0.0f);
    }

    // Verify new classic categories & icons
    assert(reg.findIcon("inst_chiptune") != nullptr);
    assert(reg.findIcon("inst_organ") != nullptr);
    assert(reg.findIcon("fx_chorus") != nullptr);
    assert(reg.findIcon("hw_midi") != nullptr);

    std::cout << " -> IconRegistry queries and all " << allIcons.size() << " vector meshes passed successfully.\n";
}

void testSvgPasteParsing() {
    std::cout << "[Test] SVG Paste parsing & validation...\n";

    // 1. Raw 'd' path string
    const char* rawPath = "M 10 10 L 30 10 L 30 30 L 10 30 Z";
    auto res1 = IconRegistry::parsePastedSvg(rawPath);
    assert(res1.has_value());
    assert(res1->svgPath == rawPath);

    // 2. Full <svg> element with viewBox and path
    const char* fullSvg = R"(<svg width="24" height="24" viewBox="0 0 48 48" fill="none"><path d="M 2 2 L 40 2 L 40 40 Z" stroke="white"/></svg>)";
    auto res2 = IconRegistry::parsePastedSvg(fullSvg);
    assert(res2.has_value());
    assert(res2->viewBox.width() == 48.0f);
    assert(res2->svgPath == "M 2 2 L 40 2 L 40 40 Z");

    // 3. Invalid or garbage text
    auto res3 = IconRegistry::parsePastedSvg("Hello world this is not SVG!");
    assert(!res3.has_value());

    auto res4 = IconRegistry::parsePastedSvg("");
    assert(!res4.has_value());

    std::cout << " -> SVG paste parsing and validation passed.\n";
}

void testTrackIconPersistence() {
    std::cout << "[Test] SequencerTrack icon property and serialization...\n";

    sequencer::StepSequencer seq;
    audio::AudioGraph graph;

    size_t trk0 = seq.addTrack("Synth Lead", 101);
    seq.getTrack(trk0)->setIconRef("preset:inst_synth");

    size_t trk1 = seq.addTrack("Kick Beat", 102);
    seq.getTrack(trk1)->setIconRef("preset:drum_kick");

    size_t trk2 = seq.addTrack("Custom Rig", 103);
    seq.getTrack(trk2)->setIconRef("svg:M 10 10 L 20 20 Z");

    // Roundtrip JSON serialization
    std::string jsonStr = project::ProjectFile::serializeJson(graph, seq, "Icon Test Project", 130.0, 0.5);
    assert(jsonStr.find("preset:inst_synth") != std::string::npos);
    assert(jsonStr.find("preset:drum_kick") != std::string::npos);
    assert(jsonStr.find("svg:M 10 10 L 20 20 Z") != std::string::npos);

    audio::AudioGraph loadedGraph;
    sequencer::StepSequencer loadedSeq;
    std::string title;
    double bpm = 0.0, swing = 0.0;
    int root = 0;
    bool minor = false;
    std::vector<theory::ChordEvent> chords;

    bool loadOk = project::ProjectFile::deserializeJson(jsonStr, loadedGraph, loadedSeq, title, bpm, swing, root, minor, chords);
    assert(loadOk);
    assert(loadedSeq.getNumTracks() == 3);
    assert(loadedSeq.getTrack(0)->getIconRef() == "preset:inst_synth");
    assert(loadedSeq.getTrack(1)->getIconRef() == "preset:drum_kick");
    assert(loadedSeq.getTrack(2)->getIconRef() == "svg:M 10 10 L 20 20 Z");

    std::cout << " -> Track icon persistence & JSON serialization passed.\n";
}

void testIconSearchDialogLogic() {
    std::cout << "[Test] IconSearchDialog filtering & paste provider...\n";

    IconSearchDialog dialog;
    dialog.open("Track 1", 0, "preset:inst_synth");
    assert(dialog.isOpen());

    // Mock clipboard
    dialog.setClipboardProvider([]() {
        return std::string("M 5 5 L 15 5 L 15 15 Z");
    });

    dialog.layout(1280, 720);

    // Simulate key input: 'k', 'i', 'c', 'k'
    dialog.handleKey('k', 0, 1, 0);
    dialog.handleKey('i', 0, 1, 0);
    dialog.handleKey('c', 0, 1, 0);
    dialog.handleKey('k', 0, 1, 0);

    // Simulate Escape to close
    dialog.handleKey(256, 0, 1, 0);
    assert(!dialog.isOpen());

    std::cout << " -> IconSearchDialog basic interactions passed.\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "Running Eatsbits Icon System Tests...\n";
    std::cout << "========================================\n";

    testIconRegistryBasics();
    testSvgPasteParsing();
    testTrackIconPersistence();
    testIconSearchDialogLogic();

    std::cout << "========================================\n";
    std::cout << "All Icon System Tests Passed!\n";
    std::cout << "========================================\n";
    return 0;
}
