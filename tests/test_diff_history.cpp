#include <iostream>
#include <cassert>
#include "../include/eatsbits/project/diff_history_manager.hpp"

using namespace eatsbits::project;

void testDiffHunkApplication() {
    std::cout << "[Test 1] Diff Hunk calculation and application..." << std::endl;

    std::string v1 = 
        "return eatsbeats.song {\n"
        "  meta = {\n"
        "    title = \"Acid Track\",\n"
        "    bpm = 130.00,\n"
        "    swing = 0.50,\n"
        "  },\n"
        "}\n";

    std::string v2 = 
        "return eatsbeats.song {\n"
        "  meta = {\n"
        "    title = \"Acid Track\",\n"
        "    bpm = 145.00,\n"
        "    swing = 0.50,\n"
        "  },\n"
        "}\n";

    auto forward = DiffHistoryManager::computeDiffHunks(v1, v2);
    assert(!forward.empty());
    assert(forward[0].oldLines.size() == 1);
    assert(forward[0].newLines.size() == 1);
    assert(forward[0].oldLines[0].find("130.00") != std::string::npos);
    assert(forward[0].newLines[0].find("145.00") != std::string::npos);

    // Apply forward to v1 -> should produce v2
    std::string appliedForward = DiffHistoryManager::applyHunks(v1, forward);
    assert(appliedForward == v2);

    // Invert hunks and apply to v2 -> should produce v1
    auto inverse = DiffHistoryManager::invertHunks(forward);
    std::string appliedInverse = DiffHistoryManager::applyHunks(v2, inverse);
    assert(appliedInverse == v1);

    std::cout << "  -> Passed: Bidirectional hunk application is exact!" << std::endl;
}

void testDiffHistoryManagerUndoRedo() {
    std::cout << "[Test 2] DiffHistoryManager Undo / Redo lifecycle..." << std::endl;

    DiffHistoryManager history;
    std::string s0 = "title = \"Song\"\nbpm = 120.0\n";
    std::string s1 = "title = \"Song\"\nbpm = 135.0\n";
    std::string s2 = "title = \"Cyber Song\"\nbpm = 135.0\n";

    history.init(s0, "Initial Project");
    assert(!history.canUndo());
    assert(!history.canRedo());
    assert(history.getPastCount() == 0);
    assert(history.getCurrentTimelineIndex() == 0);

    // Record s1
    bool rec1 = history.record(s1, "Set BPM to 135.0", "TEMPO");
    assert(rec1);
    (void)rec1;
    assert(history.canUndo());
    assert(!history.canRedo());
    assert(history.getCurrentScript() == s1);
    assert(history.getCurrentTimelineIndex() == 1);

    // Record s2
    bool rec2 = history.record(s2, "Rename to Cyber Song", "META");
    assert(rec2);
    (void)rec2;
    assert(history.canUndo());
    assert(history.getPastCount() == 2);
    assert(history.getCurrentTimelineIndex() == 2);

    // Undo step 1: s2 -> s1
    std::string restored;
    bool u1 = history.undo(restored);
    assert(u1);
    (void)u1;
    assert(restored == s1);
    assert(history.getCurrentScript() == s1);
    assert(history.canUndo());
    assert(history.canRedo());
    assert(history.getCurrentTimelineIndex() == 1);

    // Undo step 2: s1 -> s0
    bool u2 = history.undo(restored);
    assert(u2);
    (void)u2;
    assert(restored == s0);
    assert(history.getCurrentScript() == s0);
    assert(!history.canUndo());
    assert(history.canRedo());
    assert(history.getCurrentTimelineIndex() == 0);

    // Redo step 1: s0 -> s1
    std::string advanced;
    bool r1 = history.redo(advanced);
    assert(r1);
    (void)r1;
    assert(advanced == s1);
    assert(history.getCurrentScript() == s1);
    assert(history.canUndo());
    assert(history.canRedo());

    // Redo step 2: s1 -> s2
    bool r2 = history.redo(advanced);
    assert(r2);
    (void)r2;
    assert(advanced == s2);
    assert(history.getCurrentScript() == s2);
    assert(history.canUndo());
    assert(!history.canRedo());

    std::cout << "  -> Passed: Full Undo/Redo cycle verified with pure diffs!" << std::endl;
}

void testTransactionCoalescing() {
    std::cout << "[Test 3] Transaction Coalescing for Continuous Slider Gestures..." << std::endl;

    DiffHistoryManager history;
    std::string base = "volume = 0.50\n";
    history.init(base);

    // Start dragging volume slider
    history.beginTransaction("Adjust Master Volume", "VOLUME");
    assert(history.isInTransaction());

    // Intermediate 60fps moves: should not create history steps
    assert(!history.record("volume = 0.55\n", "Inter 1"));
    assert(!history.record("volume = 0.60\n", "Inter 2"));
    assert(!history.record("volume = 0.75\n", "Inter 3"));

    // Mouse release / commit
    bool committed = history.commitTransaction("volume = 0.85\n");
    assert(committed);
    (void)committed;
    assert(!history.isInTransaction());
    assert(history.getPastCount() == 1); // Exactly 1 step created!
    assert(history.getCurrentScript() == "volume = 0.85\n");

    // Undoing reverts directly to 0.50 in one step
    std::string out;
    assert(history.undo(out));
    assert(out == base);

    std::cout << "  -> Passed: Continuous slider gesture coalesced into 1 clean diff!" << std::endl;
}

void testTimelineJumpAndMilestone() {
    std::cout << "[Test 4] Multi-Step Timeline Time-Travel and Milestones..." << std::endl;

    DiffHistoryManager history;
    history.init("step = 0\n");
    history.record("step = 1\n", "Action 1");
    history.record("step = 2\n", "Action 2");
    history.createMilestone("Drop Prepared");
    history.record("step = 3\n", "Action 3");
    history.record("step = 4\n", "Action 4");

    assert(history.getTimelineCount() == 5);
    assert(history.getCurrentTimelineIndex() == 4);

    // Jump back 3 steps to index 1 ("Action 1")
    std::string restored;
    bool jumped = history.jumpToTimelineIndex(1, restored);
    assert(jumped);
    assert(restored == "step = 1\n");
    assert(history.getCurrentTimelineIndex() == 1);
    assert(history.canUndo());
    assert(history.canRedo());

    // Jump forward to index 3 ("Action 3")
    jumped = history.jumpToTimelineIndex(3, restored);
    assert(jumped);
    assert(restored == "step = 3\n");
    assert(history.getCurrentTimelineIndex() == 3);

    auto timeline = history.getTimeline();
    assert(timeline.size() == 5);
    assert(timeline[2].isMilestone);
    assert(timeline[2].milestoneName == "Drop Prepared");

    std::cout << "  -> Passed: Timeline time-travel and milestones verified!" << std::endl;
}

void testCompactDiffFormatting() {
    std::cout << "[Test 5] Compact Diff Hunk Formatting for UI Display..." << std::endl;

    std::string oldT = "A\nB\nC\nD\nE\nF\nG\nH\nI\nJ\n";
    std::string newT = "A\nB\nC\nD\nEDITED_E\nF\nG\nH\nI\nJ\n";

    auto compact = DiffHistoryManager::computeCompactDiff(oldT, newT, 1);
    assert(!compact.empty());

    bool foundRemoved = false;
    bool foundAdded = false;
    for (const auto& line : compact) {
        if (line.type == DiffLineType::Removed && line.text == "E") foundRemoved = true;
        if (line.type == DiffLineType::Added && line.text == "EDITED_E") foundAdded = true;
    }
    assert(foundRemoved);
    assert(foundAdded);

    std::cout << "  -> Passed: Compact diff accurately highlights changed lines!" << std::endl;
}

int main() {
    std::cout << "============================================================" << std::endl;
    std::cout << "  Eatsbits Pure Diff-Based History Manager Unit Test Suite  " << std::endl;
    std::cout << "============================================================" << std::endl;

    testDiffHunkApplication();
    testDiffHistoryManagerUndoRedo();
    testTransactionCoalescing();
    testTimelineJumpAndMilestone();
    testCompactDiffFormatting();

    std::cout << "\n[+] ALL 5 HISTORY MANAGER TESTS PASSED SUCCESSFULLY!" << std::endl;
    return 0;
}
