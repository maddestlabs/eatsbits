#include "../../include/eatsbits/project/diff_history_manager.hpp"

#include <sstream>
#include <algorithm>
#include <chrono>

namespace eatsbits::project {

namespace {

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::stringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    return lines;
}

std::string joinLines(const std::vector<std::string>& lines) {
    std::string out;
    for (size_t i = 0; i < lines.size(); ++i) {
        out += lines[i];
        if (i + 1 < lines.size() || !lines.empty()) {
            out += "\n";
        }
    }
    return out;
}

// LCS table for slice of lines
std::vector<std::pair<int, int>> computeLcs(
    const std::vector<std::string>& a, int aStart, int aEnd,
    const std::vector<std::string>& b, int bStart, int bEnd) {

    int n = aEnd - aStart;
    int m = bEnd - bStart;
    if (n <= 0 || m <= 0) return {};

    std::vector<std::vector<int>> dp(n + 1, std::vector<int>(m + 1, 0));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < m; ++j) {
            if (a[aStart + i] == b[bStart + j]) {
                dp[i + 1][j + 1] = dp[i][j] + 1;
            } else {
                dp[i + 1][j + 1] = std::max(dp[i + 1][j], dp[i][j + 1]);
            }
        }
    }

    std::vector<std::pair<int, int>> matches;
    int i = n, j = m;
    while (i > 0 && j > 0) {
        if (a[aStart + i - 1] == b[bStart + j - 1]) {
            matches.push_back({aStart + i - 1, bStart + j - 1});
            --i;
            --j;
        } else if (dp[i - 1][j] >= dp[i][j - 1]) {
            --i;
        } else {
            --j;
        }
    }
    std::reverse(matches.begin(), matches.end());
    return matches;
}

} // anonymous namespace

DiffHistoryManager::DiffHistoryManager(size_t maxDepth)
    : maxDepth_(maxDepth > 0 ? maxDepth : kDefaultMaxDepth) {
}

void DiffHistoryManager::init(const std::string& initialScript, const std::string& initialDescription) {
    currentScript_ = initialScript;
    past_.clear();
    future_.clear();
    inTransaction_ = false;

    HistoryDiffEntry entry;
    entry.id = "init_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    entry.description = initialDescription;
    entry.category = "INIT";
    entry.timestamp = std::chrono::system_clock::now();
    entry.isMilestone = true;
    entry.milestoneName = "Initial Project";
    entry.stateFingerprint = computeFingerprint(initialScript);

    currentEntry_ = entry;
}

bool DiffHistoryManager::record(
    const std::string& newScript,
    const std::string& description,
    const std::string& category,
    bool isMilestone,
    const std::string& milestoneName,
    bool force) {

    if (inTransaction_) {
        // Coalesced into active transaction
        return false;
    }

    uint32_t newFp = computeFingerprint(newScript);
    if (!force && currentEntry_ && currentEntry_->stateFingerprint == newFp && currentScript_ == newScript) {
        // Identical state, skip recording
        return false;
    }

    auto forward = computeDiffHunks(currentScript_, newScript);
    auto inverse = invertHunks(forward);

    if (!force && forward.empty()) {
        return false;
    }

    if (currentEntry_) {
        past_.push_back(*currentEntry_);
        if (past_.size() > maxDepth_) {
            past_.erase(past_.begin());
        }
    }

    future_.clear();

    HistoryDiffEntry newEntry;
    newEntry.id = "step_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    newEntry.description = description;
    newEntry.category = category;
    newEntry.timestamp = std::chrono::system_clock::now();
    newEntry.forwardHunks = std::move(forward);
    newEntry.inverseHunks = std::move(inverse);
    newEntry.isMilestone = isMilestone;
    newEntry.milestoneName = milestoneName;
    newEntry.stateFingerprint = newFp;

    currentScript_ = newScript;
    currentEntry_ = std::move(newEntry);
    return true;
}

void DiffHistoryManager::beginTransaction(const std::string& description, const std::string& category) {
    if (inTransaction_) return;
    inTransaction_ = true;
    transactionDescription_ = description;
    transactionCategory_ = category;
    transactionInitialScript_ = currentScript_;
    transactionInitialFingerprint_ = computeFingerprint(currentScript_);
}

bool DiffHistoryManager::commitTransaction(const std::string& finalScript) {
    if (!inTransaction_) return false;
    inTransaction_ = false;

    uint32_t finalFp = computeFingerprint(finalScript);
    if (finalFp == transactionInitialFingerprint_ && finalScript == transactionInitialScript_) {
        // Parameter returned to original value, nothing changed
        return false;
    }

    auto forward = computeDiffHunks(transactionInitialScript_, finalScript);
    auto inverse = invertHunks(forward);

    if (forward.empty()) {
        return false;
    }

    if (currentEntry_) {
        past_.push_back(*currentEntry_);
        if (past_.size() > maxDepth_) {
            past_.erase(past_.begin());
        }
    }

    future_.clear();

    HistoryDiffEntry newEntry;
    newEntry.id = "tx_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    newEntry.description = transactionDescription_.empty() ? "Adjust Parameter" : transactionDescription_;
    newEntry.category = transactionCategory_.empty() ? "TUNE" : transactionCategory_;
    newEntry.timestamp = std::chrono::system_clock::now();
    newEntry.forwardHunks = std::move(forward);
    newEntry.inverseHunks = std::move(inverse);
    newEntry.isMilestone = false;
    newEntry.stateFingerprint = finalFp;

    currentScript_ = finalScript;
    currentEntry_ = std::move(newEntry);
    return true;
}

void DiffHistoryManager::cancelTransaction() noexcept {
    inTransaction_ = false;
    transactionDescription_.clear();
    transactionCategory_.clear();
    transactionInitialScript_.clear();
    transactionInitialFingerprint_ = 0;
}

bool DiffHistoryManager::undo(std::string& outRestoredScript) {
    if (past_.empty() || !currentEntry_) return false;

    // To revert the current state to the previous state, apply currentEntry_'s inverseHunks
    std::string restored = applyHunks(currentScript_, currentEntry_->inverseHunks);

    future_.push_back(*currentEntry_);
    *currentEntry_ = past_.back();
    past_.pop_back();

    currentScript_ = restored;
    outRestoredScript = restored;
    return true;
}

bool DiffHistoryManager::redo(std::string& outRestoredScript) {
    if (future_.empty() || !currentEntry_) return false;

    HistoryDiffEntry next = future_.back();
    future_.pop_back();

    // To advance to next state, apply next's forwardHunks
    std::string advanced = applyHunks(currentScript_, next.forwardHunks);

    past_.push_back(*currentEntry_);
    *currentEntry_ = next;

    currentScript_ = advanced;
    outRestoredScript = advanced;
    return true;
}

bool DiffHistoryManager::jumpToTimelineIndex(size_t targetIndex, std::string& outRestoredScript) {
    size_t currentIndex = getCurrentTimelineIndex();
    if (targetIndex == currentIndex) {
        outRestoredScript = currentScript_;
        return true;
    }

    size_t total = getTimelineCount();
    if (targetIndex >= total) return false;

    if (targetIndex < currentIndex) {
        size_t stepsToUndo = currentIndex - targetIndex;
        for (size_t s = 0; s < stepsToUndo; ++s) {
            std::string dummy;
            if (!undo(dummy)) return false;
        }
    } else {
        size_t stepsToRedo = targetIndex - currentIndex;
        for (size_t s = 0; s < stepsToRedo; ++s) {
            std::string dummy;
            if (!redo(dummy)) return false;
        }
    }

    outRestoredScript = currentScript_;
    return true;
}

void DiffHistoryManager::createMilestone(const std::string& name) {
    if (!currentEntry_) return;
    currentEntry_->isMilestone = true;
    currentEntry_->milestoneName = name.empty() ? "Checkpoint" : name;
}

void DiffHistoryManager::clear() {
    past_.clear();
    future_.clear();
    if (currentEntry_) {
        currentEntry_->forwardHunks.clear();
        currentEntry_->inverseHunks.clear();
        currentEntry_->description = "Baseline";
        currentEntry_->isMilestone = true;
        currentEntry_->milestoneName = "Origin";
    }
}

size_t DiffHistoryManager::getTimelineCount() const noexcept {
    return past_.size() + (currentEntry_ ? 1 : 0) + future_.size();
}

size_t DiffHistoryManager::getCurrentTimelineIndex() const noexcept {
    return currentEntry_ ? past_.size() : 0;
}

std::vector<HistoryDiffEntry> DiffHistoryManager::getTimeline() const {
    std::vector<HistoryDiffEntry> timeline;
    timeline.reserve(getTimelineCount());
    for (const auto& entry : past_) {
        timeline.push_back(entry);
    }
    if (currentEntry_) {
        timeline.push_back(*currentEntry_);
    }
    // future_ is stack where top is back(), so iterate in reverse for chronological order
    for (auto it = future_.rbegin(); it != future_.rend(); ++it) {
        timeline.push_back(*it);
    }
    return timeline;
}

std::vector<DiffHunk> DiffHistoryManager::computeDiffHunks(const std::string& oldText, const std::string& newText) {
    auto oldLines = splitLines(oldText);
    auto newLines = splitLines(newText);

    if (oldLines == newLines) return {};

    // 1. Prefix trim
    int prefix = 0;
    while (prefix < static_cast<int>(oldLines.size()) &&
           prefix < static_cast<int>(newLines.size()) &&
           oldLines[prefix] == newLines[prefix]) {
        ++prefix;
    }

    // 2. Suffix trim
    int oldSuffix = static_cast<int>(oldLines.size()) - 1;
    int newSuffix = static_cast<int>(newLines.size()) - 1;
    while (oldSuffix >= prefix && newSuffix >= prefix && oldLines[oldSuffix] == newLines[newSuffix]) {
        --oldSuffix;
        --newSuffix;
    }

    std::vector<DiffHunk> hunks;

    // If everything inside prefix..suffix is modified
    DiffHunk hunk;
    hunk.oldStartLine = prefix + 1; // 1-indexed
    hunk.oldCount = (oldSuffix >= prefix) ? (oldSuffix - prefix + 1) : 0;
    hunk.newStartLine = prefix + 1;
    hunk.newCount = (newSuffix >= prefix) ? (newSuffix - prefix + 1) : 0;

    for (int i = prefix; i <= oldSuffix; ++i) {
        hunk.oldLines.push_back(oldLines[i]);
    }
    for (int j = prefix; j <= newSuffix; ++j) {
        hunk.newLines.push_back(newLines[j]);
    }

    hunks.push_back(std::move(hunk));
    return hunks;
}

std::vector<DiffHunk> DiffHistoryManager::invertHunks(const std::vector<DiffHunk>& hunks) {
    std::vector<DiffHunk> inv;
    inv.reserve(hunks.size());
    for (const auto& h : hunks) {
        DiffHunk rev;
        rev.oldStartLine = h.newStartLine;
        rev.oldCount = h.newCount;
        rev.newStartLine = h.oldStartLine;
        rev.newCount = h.oldCount;
        rev.oldLines = h.newLines;
        rev.newLines = h.oldLines;
        inv.push_back(std::move(rev));
    }
    return inv;
}

std::string DiffHistoryManager::applyHunks(const std::string& baseText, const std::vector<DiffHunk>& hunks) {
    if (hunks.empty()) return baseText;

    auto lines = splitLines(baseText);

    // Apply hunks from bottom to top so line index offsets do not perturb earlier hunks
    auto sortedHunks = hunks;
    std::sort(sortedHunks.begin(), sortedHunks.end(), [](const DiffHunk& a, const DiffHunk& b) {
        return a.oldStartLine > b.oldStartLine;
    });

    for (const auto& h : sortedHunks) {
        int idx = h.oldStartLine - 1;
        if (idx < 0) idx = 0;
        if (idx > static_cast<int>(lines.size())) idx = static_cast<int>(lines.size());

        int removeCount = h.oldCount;
        if (idx + removeCount > static_cast<int>(lines.size())) {
            removeCount = static_cast<int>(lines.size()) - idx;
        }

        // Erase old lines
        if (removeCount > 0) {
            lines.erase(lines.begin() + idx, lines.begin() + idx + removeCount);
        }

        // Insert new lines
        lines.insert(lines.begin() + idx, h.newLines.begin(), h.newLines.end());
    }

    return joinLines(lines);
}

std::vector<DiffLine> DiffHistoryManager::computeDetailedDiff(const std::string& oldText, const std::string& newText) {
    auto oldLines = splitLines(oldText);
    auto newLines = splitLines(newText);
    std::vector<DiffLine> result;

    int oldIdx = 0, newIdx = 0;
    while (oldIdx < static_cast<int>(oldLines.size()) && newIdx < static_cast<int>(newLines.size())) {
        if (oldLines[oldIdx] == newLines[newIdx]) {
            result.push_back({oldLines[oldIdx], DiffLineType::Unchanged, oldIdx + 1, newIdx + 1});
            ++oldIdx;
            ++newIdx;
        } else {
            // Find lookahead matches
            int matchNew = -1;
            for (int k = newIdx + 1; k < static_cast<int>(newLines.size()) && k < newIdx + 8; ++k) {
                if (newLines[k] == oldLines[oldIdx]) {
                    matchNew = k;
                    break;
                }
            }

            int matchOld = -1;
            for (int k = oldIdx + 1; k < static_cast<int>(oldLines.size()) && k < oldIdx + 8; ++k) {
                if (oldLines[k] == newLines[newIdx]) {
                    matchOld = k;
                    break;
                }
            }

            if (matchNew != -1 && (matchOld == -1 || (matchNew - newIdx) <= (matchOld - oldIdx))) {
                while (newIdx < matchNew) {
                    result.push_back({newLines[newIdx], DiffLineType::Added, 0, newIdx + 1});
                    ++newIdx;
                }
            } else if (matchOld != -1) {
                while (oldIdx < matchOld) {
                    result.push_back({oldLines[oldIdx], DiffLineType::Removed, oldIdx + 1, 0});
                    ++oldIdx;
                }
            } else {
                result.push_back({oldLines[oldIdx], DiffLineType::Removed, oldIdx + 1, 0});
                result.push_back({newLines[newIdx], DiffLineType::Added, 0, newIdx + 1});
                ++oldIdx;
                ++newIdx;
            }
        }
    }

    while (oldIdx < static_cast<int>(oldLines.size())) {
        result.push_back({oldLines[oldIdx], DiffLineType::Removed, oldIdx + 1, 0});
        ++oldIdx;
    }

    while (newIdx < static_cast<int>(newLines.size())) {
        result.push_back({newLines[newIdx], DiffLineType::Added, 0, newIdx + 1});
        ++newIdx;
    }

    return result;
}

std::vector<DiffLine> DiffHistoryManager::computeCompactDiff(const std::string& oldText, const std::string& newText, int contextLines) {
    auto full = computeDetailedDiff(oldText, newText);
    if (full.empty()) return {};

    bool hasChanges = false;
    for (const auto& l : full) {
        if (l.type != DiffLineType::Unchanged) {
            hasChanges = true;
            break;
        }
    }
    if (!hasChanges) {
        return {{"No code changes between states", DiffLineType::Unchanged, 0, 0}};
    }

    std::vector<bool> keep(full.size(), false);
    for (int i = 0; i < static_cast<int>(full.size()); ++i) {
        if (full[i].type != DiffLineType::Unchanged) {
            for (int c = -contextLines; c <= contextLines; ++c) {
                int target = i + c;
                if (target >= 0 && target < static_cast<int>(full.size())) {
                    keep[target] = true;
                }
            }
        }
    }

    std::vector<DiffLine> compact;
    int skipped = 0;
    for (int i = 0; i < static_cast<int>(full.size()); ++i) {
        if (keep[i]) {
            if (skipped > 0) {
                compact.push_back({"@@ ... " + std::to_string(skipped) + " lines unchanged ... @@", DiffLineType::Unchanged, 0, 0});
                skipped = 0;
            }
            compact.push_back(full[i]);
        } else {
            ++skipped;
        }
    }
    if (skipped > 0) {
        compact.push_back({"@@ ... " + std::to_string(skipped) + " lines unchanged ... @@", DiffLineType::Unchanged, 0, 0});
    }

    return compact;
}

uint32_t DiffHistoryManager::computeFingerprint(const std::string& text) noexcept {
    // 32-bit FNV-1a hash
    uint32_t hash = 2166136261u;
    for (unsigned char c : text) {
        hash ^= c;
        hash *= 16777619u;
    }
    return hash;
}

} // namespace eatsbits::project
