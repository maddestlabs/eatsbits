#ifndef EATS_DIFF_HISTORY_MANAGER_HPP
#define EATS_DIFF_HISTORY_MANAGER_HPP

#include <string>
#include <vector>
#include <deque>
#include <chrono>
#include <optional>
#include <cstdint>

namespace eatsbits::project {

enum class DiffLineType {
    Unchanged,
    Added,
    Removed
};

struct DiffLine {
    std::string text;
    DiffLineType type{DiffLineType::Unchanged};
    int oldLineNumber{0};
    int newLineNumber{0};
};

struct DiffHunk {
    int oldStartLine{0};
    int oldCount{0};
    int newStartLine{0};
    int newCount{0};
    std::vector<std::string> oldLines;
    std::vector<std::string> newLines;
};

struct HistoryDiffEntry {
    std::string id;
    std::string description;
    std::string category{"EDIT"};
    std::chrono::system_clock::time_point timestamp;
    std::vector<DiffHunk> forwardHunks; // Redo delta (old -> new)
    std::vector<DiffHunk> inverseHunks; // Undo delta (new -> old)
    bool isMilestone{false};
    std::string milestoneName;
    uint32_t stateFingerprint{0};
};

/**
 * Pure Diff-Based In-Memory History Manager for Eatsbeats/Eatsbits.
 * Stores canonical base project Eatscript and stores deltas as bidirectional
 * diff hunks. Features sub-millisecond undo/redo, timeline time-travel,
 * gesture transaction coalescing, and visual diff inspection.
 */
class DiffHistoryManager {
public:
    static constexpr size_t kDefaultMaxDepth = 100;

    explicit DiffHistoryManager(size_t maxDepth = kDefaultMaxDepth);
    ~DiffHistoryManager() = default;

    /// Initializes history with the baseline project script.
    void init(const std::string& initialScript, const std::string& initialDescription = "Project Initialized");

    /// Records a new project state change.
    bool record(
        const std::string& newScript,
        const std::string& description,
        const std::string& category = "EDIT",
        bool isMilestone = false,
        const std::string& milestoneName = "",
        bool force = false);

    /// Begins a continuous gesture transaction (e.g. knob turn / fader drag).
    void beginTransaction(const std::string& description, const std::string& category = "TUNE");

    /// Commits a continuous gesture transaction with the final script.
    bool commitTransaction(const std::string& finalScript);

    /// Cancels an active transaction without recording an undo step.
    void cancelTransaction() noexcept;

    /// Performs Undo: rolls back one diff delta and outputs the restored script.
    bool undo(std::string& outRestoredScript);

    /// Performs Redo: rolls forward one diff delta and outputs the restored script.
    bool redo(std::string& outRestoredScript);

    /// Jumps directly to any step in the chronological timeline.
    bool jumpToTimelineIndex(size_t targetIndex, std::string& outRestoredScript);

    /// Creates a named milestone / checkpoint at current state.
    void createMilestone(const std::string& name);

    /// Clears the history stack, retaining current state.
    void clear();

    [[nodiscard]] bool canUndo() const noexcept { return !past_.empty(); }
    [[nodiscard]] bool canRedo() const noexcept { return !future_.empty(); }
    [[nodiscard]] bool isInTransaction() const noexcept { return inTransaction_; }

    [[nodiscard]] const std::string& getCurrentScript() const noexcept { return currentScript_; }
    [[nodiscard]] const HistoryDiffEntry* getCurrentEntry() const noexcept { return currentEntry_ ? &(*currentEntry_) : nullptr; }

    [[nodiscard]] size_t getPastCount() const noexcept { return past_.size(); }
    [[nodiscard]] size_t getFutureCount() const noexcept { return future_.size(); }
    [[nodiscard]] size_t getTimelineCount() const noexcept;
    [[nodiscard]] size_t getCurrentTimelineIndex() const noexcept;

    [[nodiscard]] std::vector<HistoryDiffEntry> getTimeline() const;

    /// Computes hunk-based diff between oldText and newText.
    static std::vector<DiffHunk> computeDiffHunks(const std::string& oldText, const std::string& newText);

    /// Inverts forward hunks to create reverse hunks for undo.
    static std::vector<DiffHunk> invertHunks(const std::vector<DiffHunk>& hunks);

    /// Applies hunks to baseText to produce the updated text.
    static std::string applyHunks(const std::string& baseText, const std::vector<DiffHunk>& hunks);

    /// Detailed line-by-line diff for UI inspection.
    static std::vector<DiffLine> computeDetailedDiff(const std::string& oldText, const std::string& newText);

    /// Compact hunk-based diff with context lines for UI visualization.
    static std::vector<DiffLine> computeCompactDiff(const std::string& oldText, const std::string& newText, int contextLines = 2);

    /// Fast 32-bit FNV-1a fingerprint for quick duplicate state rejection.
    static uint32_t computeFingerprint(const std::string& text) noexcept;

private:
    size_t maxDepth_;
    std::string currentScript_;
    std::optional<HistoryDiffEntry> currentEntry_;
    std::deque<HistoryDiffEntry> past_;
    std::deque<HistoryDiffEntry> future_; // top of stack (last element) is next redo

    bool inTransaction_{false};
    std::string transactionDescription_;
    std::string transactionCategory_;
    std::string transactionInitialScript_;
    uint32_t transactionInitialFingerprint_{0};
};

} // namespace eatsbits::project

#endif // EATS_DIFF_HISTORY_MANAGER_HPP
